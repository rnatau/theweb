"""Reject embedded debug information and private symbols in PE/ELF/Mach-O releases."""
import argparse
import hashlib
import json
from pathlib import Path
import struct


PE_RELEASE_METADATA = {12: 'VC_FEATURE', 13: 'POGO', 16: 'REPRO', 20: 'EX_DLLCHARACTERISTICS'}
PE_DEBUG_INFORMATION = {1: 'COFF', 2: 'CODEVIEW/PDB', 4: 'MISC/DBG', 17: 'EMBEDDED_PDB', 19: 'PDB_CHECKSUM'}


def pe_pogo_groups(payload, path):
    if not payload:
        return 0
    # MSVC COFF-group records: signature, then aligned RVA/size/name entries.
    # Signature constants are DWORDs, stored little endian (LTCG is b'GCTL').
    signatures = {0, 0x4c544347, 0x50474900, 0x50474f00, 0x50475500, 0x5350474f}
    assert len(payload) >= 4 and struct.unpack_from('<I', payload)[0] in signatures, \
        f'{path}: invalid PE POGO signature (type 13)'
    cursor, count = 4, 0
    while cursor < len(payload):
        assert cursor + 8 < len(payload), f'{path}: truncated PE POGO group record'
        end = payload.find(b'\0', cursor + 8)
        assert end >= 0, f'{path}: unterminated PE POGO group name'
        cursor = (end + 4) & ~3
        assert cursor <= len(payload), f'{path}: truncated PE POGO alignment'
        count += 1
    return count


def pe_debug_metadata(data, pe, optional, path):
    magic = struct.unpack_from('<H', data, pe + 24)[0]
    assert magic in (0x10b, 0x20b), f'{path}: unsupported PE optional header'
    directories = 112 if magic == 0x20b else 96
    assert optional >= directories, f'{path}: truncated PE optional header'
    directory_count = struct.unpack_from('<I', data, pe + 24 + directories - 4)[0]
    if directory_count <= 6:
        return []
    assert optional >= directories + 7 * 8, f'{path}: truncated PE data directories'
    directory = pe + 24 + directories + 6 * 8
    rva, size = struct.unpack_from('<II', data, directory)
    if (rva, size) == (0, 0):
        return []
    assert rva and size and size % 28 == 0, f'{path}: malformed PE debug directory'
    section_count = struct.unpack_from('<H', data, pe + 6)[0]
    offset = None
    # Data directories use RVAs; debug payload pointers are file offsets.
    for i in range(section_count):
        section = pe + 24 + optional + i * 40
        virtual_size, address, raw_size, raw_offset = struct.unpack_from('<IIII', data, section + 8)
        if address <= rva and rva - address + size <= raw_size:
            offset = raw_offset + rva - address
            break
    if offset is None:
        header_size = struct.unpack_from('<I', data, pe + 24 + 60)[0]
        if rva + size <= header_size:
            offset = rva
    assert offset is not None and offset + size <= len(data), f'{path}: unmapped PE debug directory'
    metadata = []
    for entry in range(offset, offset + size, 28):
        kind, payload_size, payload_rva, payload_offset = struct.unpack_from('<IIII', data, entry + 12)
        name = PE_RELEASE_METADATA.get(kind, PE_DEBUG_INFORMATION.get(kind, 'UNKNOWN'))
        assert kind in PE_RELEASE_METADATA, f'{path}: PE debug information remains: {name} (type {kind})'
        assert not payload_size or (payload_offset and payload_offset + payload_size <= len(data)), \
            f'{path}: truncated PE {name} payload'
        record = {'type': kind, 'name': name, 'bytes': payload_size}
        if kind == 13:
            record['coff_groups'] = pe_pogo_groups(data[payload_offset:payload_offset + payload_size], path)
        metadata.append(record)
    return metadata


def inspect_macho(data, path):
    assert len(data) >= 32, f'{path}: truncated Mach-O header'
    cpu, subtype, filetype, count, size = struct.unpack_from('<IIIII', data, 4)
    assert cpu in (0x01000007, 0x0100000c), f'{path}: expected x64 or arm64 Mach-O'
    assert filetype in (2, 6), f'{path}: expected Mach-O executable or dylib'
    end = 32 + size
    assert end <= len(data), f'{path}: truncated Mach-O load commands'
    cursor = 32
    for _ in range(count):
        assert cursor + 8 <= end, f'{path}: truncated Mach-O load command'
        command, length = struct.unpack_from('<II', data, cursor)
        assert length >= 8 and cursor + length <= end, f'{path}: malformed Mach-O load command'
        if command == 0x19:  # LC_SEGMENT_64
            assert length >= 72, f'{path}: truncated Mach-O segment'
            segment = data[cursor + 8:cursor + 24].rstrip(b'\0')
            assert segment != b'__DWARF', f'{path}: debug sections remain'
            sections = struct.unpack_from('<I', data, cursor + 64)[0]
            assert 72 + sections * 80 <= length, f'{path}: truncated Mach-O sections'
            for index in range(sections):
                section = cursor + 72 + index * 80
                name = data[section:section + 16].rstrip(b'\0')
                flags = struct.unpack_from('<I', data, section + 64)[0]
                assert not flags & 0x02000000 and not name.startswith((b'__debug', b'__zdebug')), \
                    f'{path}: debug sections remain'
        elif command == 2:  # LC_SYMTAB: exported/dynamic symbols must remain usable.
            assert length >= 24, f'{path}: truncated Mach-O symbol command'
            offset, symbols, string_offset, string_size = struct.unpack_from('<IIII', data, cursor + 8)
            assert offset + symbols * 16 <= len(data), f'{path}: truncated Mach-O symbol table'
            assert string_offset + string_size <= len(data), f'{path}: truncated Mach-O string table'
            strings = data[string_offset:string_offset + string_size]
            for index in range(symbols):
                name_offset, kind, section, description, value = struct.unpack_from(
                    '<IBBHQ', data, offset + index * 16)
                assert name_offset < string_size, f'{path}: invalid Mach-O symbol string offset'
                name_end = strings.find(b'\0', name_offset)
                assert name_end >= 0, f'{path}: unterminated Mach-O symbol name'
                # Apple strip -S -x inserts this exact N_OPT sentinel at index zero
                # to keep indirect symbols away from index zero (rdar 5614542).
                # It carries no debug information; all other STABS remain forbidden.
                # https://github.com/apple-oss-distributions/cctools/blob/main/misc/strip.c
                if (index == 0 and (kind, section, description, value) == (0x3c, 0, 0, 0x05614542)
                        and strings[name_offset:name_end] == b'radr://5614542'):
                    continue
                assert not kind & 0xe0, f'{path}: Mach-O debug symbols remain'
                assert kind & 1 or (kind & 0x0e) == 0, f'{path}: Mach-O private symbols remain'
        cursor += length
    assert cursor == end, f'{path}: malformed Mach-O command count'
    return 'arm64' if cpu == 0x0100000c else 'x64'


def inspect(path):
    data = path.read_bytes()
    metadata = []
    if data[:2] == b'MZ':
        pe = struct.unpack_from('<I', data, 0x3c)[0]
        assert data[pe:pe + 4] == b'PE\0\0', f'{path}: invalid PE header'
        count = struct.unpack_from('<H', data, pe + 6)[0]
        symbols, symbol_count = struct.unpack_from('<II', data, pe + 12)
        optional = struct.unpack_from('<H', data, pe + 20)[0]
        assert symbols == symbol_count == 0, f'{path}: COFF symbols remain'
        sections = [data[pe + 24 + optional + i * 40:pe + 32 + optional + i * 40].rstrip(b'\0')
                    for i in range(count)]
        assert all(not name.startswith((b'.debug', b'.zdebug', b'/')) for name in sections), f'{path}: debug sections remain'
        metadata = pe_debug_metadata(data, pe, optional, path)
        kind = 'PE'
    elif data[:4] == b'\x7fELF':
        assert data[4:6] == b'\x02\x01', f'{path}: expected ELF64 little endian'
        offset = struct.unpack_from('<Q', data, 40)[0]
        size, count, names_index = struct.unpack_from('<HHH', data, 58)
        headers = [struct.unpack_from('<IIQQQQIIQQ', data, offset + i * size) for i in range(count)]
        names = headers[names_index]
        strings = data[names[4]:names[4] + names[5]]
        sections = [strings[h[0]:].split(b'\0', 1)[0] for h in headers]
        assert not any(h[1] == 2 for h in headers), f'{path}: static symbol table remains'
        assert all(not name.startswith((b'.debug', b'.zdebug', b'.stab')) for name in sections), f'{path}: debug sections remain'
        kind = 'ELF'
    elif data[:4] == b'\xcf\xfa\xed\xfe':
        architecture = inspect_macho(data, path)
        return {'path': str(path), 'format': 'Mach-O', 'architecture': architecture, 'bytes': len(data),
                'sha256': hashlib.sha256(data).hexdigest(), 'debug_information': False,
                'static_symbols': False, 'pe_debug_metadata': []}
    else:
        raise AssertionError(f'{path}: unrecognized executable format')
    return {'path': str(path), 'format': kind, 'bytes': len(data),
            'sha256': hashlib.sha256(data).hexdigest(), 'debug_information': False, 'static_symbols': False,
            'pe_debug_metadata': metadata}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binaries', nargs='+', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    results = [inspect(path) for path in args.binaries]
    if args.output:
        args.output.write_text(json.dumps({'passed': True, 'binaries': results}, indent=2) + '\n', encoding='utf-8')
    for result in results:
        print(f"PASS: {result['path']} ({result['bytes']:,} bytes), no debug information or static symbols")
        if result['pe_debug_metadata']:
            print('  PE release metadata: ' + ', '.join(entry['name'] for entry in result['pe_debug_metadata']))


if __name__ == '__main__':
    main()
