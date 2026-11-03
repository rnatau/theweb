"""Synthetic release packaging checks; no game media or GitHub access."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
import zipfile


def load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


HERE = Path(__file__).resolve().parent
packager = load("packager", HERE / "package-release.py")
publisher = load("publisher", HERE / "publish-release.py")
VERIFIER = (HERE / "verify-release-binaries.py" if (HERE / "verify-release-binaries.py").exists()
            else HERE.parent / "tools" / "verify-release-binaries.py")
TAG, COMMIT = "v2026.11.03-rc1", "a" * 40
verifier = load("verifier", VERIFIER)


def pe_image(entries=(), pe32=False):
    image = bytearray(4096)
    image[:2] = b"MZ"
    struct.pack_into("<I", image, 0x3c, 0x80)
    image[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", image, 0x86, 1)
    optional = 0xe0 if pe32 else 0xf0
    struct.pack_into("<H", image, 0x94, optional)
    struct.pack_into("<H", image, 0x98, 0x10b if pe32 else 0x20b)
    struct.pack_into("<I", image, 0x98 + 60, 0x200)
    directories = 96 if pe32 else 112
    struct.pack_into("<I", image, 0x98 + directories - 4, 16)
    section = 0x98 + optional
    image[section:section + 8] = b".rdata\0\0"
    struct.pack_into("<IIII", image, section + 8, 0xe00, 0x2000, 0xe00, 0x200)
    if entries:
        struct.pack_into("<II", image, 0x98 + directories + 6 * 8, 0x2000, len(entries) * 28)
        payload_offset = 0x800
        for index, (kind, payload) in enumerate(entries):
            struct.pack_into("<IIII", image, 0x200 + index * 28 + 12, kind, len(payload),
                             0x2000 + payload_offset - 0x200 if payload else 0,
                             payload_offset if payload else 0)
            image[payload_offset:payload_offset + len(payload)] = payload
            payload_offset += len(payload)
    return image


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        (self.root / "tools").mkdir()
        (self.root / "tools/verify-release-binaries.py").write_bytes(VERIFIER.read_bytes())
        (self.root / "README.md").write_text("Supply your own data/PC_DAT.", encoding="utf-8")
        (self.root / "LICENSE").write_text("MIT", encoding="utf-8")
        self.build = self.root / "build"
        self.build.mkdir()
        # Deliberately include private/unshipped content beside the binaries.
        (self.build / "data").mkdir()
        (self.build / "data/private.dat").write_bytes(b"private media")
        (self.build / "theweb-tools.exe").write_bytes(b"development tool")
        (self.build / "runtime.dll").write_bytes(b"unapproved DLL")
        pe = pe_image()
        elf = bytearray(128)
        elf[:6] = b"\x7fELF\x02\x01"
        struct.pack_into("<Q", elf, 40, 64)
        struct.pack_into("<HHH", elf, 58, 64, 1, 0)
        for name in ("theweb", "theweb-enhanced"):
            (self.build / name).write_bytes(elf)
            (self.build / (name + ".exe")).write_bytes(pe)
        self.destination = self.root / "dist"

    def package(self, platform):
        with patch.object(packager.subprocess, "run", return_value=subprocess.CompletedProcess(
                [], 0, stdout="edition test version\n")) as launch:
            result = packager.package(self.root, self.build, self.destination, platform, TAG, COMMIT)
        self.assertEqual(launch.call_count, 4)
        return result

    def test_archive_allowlist_checksums_and_linux_modes(self):
        for platform in ("windows-x64", "linux-x64"):
            with self.subTest(platform=platform):
                archive = self.package(platform)
                prefix = f"TheWeb-{TAG}-{platform}/"
                if platform == "windows-x64":
                    with zipfile.ZipFile(archive) as source:
                        files = {name.removeprefix(prefix): source.read(name) for name in source.namelist()}
                    names = {"theweb.exe", "theweb-enhanced.exe"}
                else:
                    with tarfile.open(archive) as source:
                        files = {entry.name.removeprefix(prefix): source.extractfile(entry).read()
                                 for entry in source.getmembers()}
                        self.assertEqual(source.getmember(prefix + "theweb").mode, 0o755)
                        self.assertEqual(source.getmember(prefix + "theweb-enhanced").mode, 0o755)
                    names = {"theweb", "theweb-enhanced"}
                self.assertEqual(set(files), names | {"README.md", "LICENSE", "build-info.json", "SHA256SUMS"})
                metadata = json.loads(files["build-info.json"])
                self.assertEqual(metadata["commit"], COMMIT)
                self.assertEqual(set(metadata["versions"]), names)
                for line in files["SHA256SUMS"].decode().splitlines():
                    digest, name = line.split("  ")
                    self.assertEqual(digest, hashlib.sha256(files[name]).hexdigest())

    def test_both_artifacts_are_required_and_verified(self):
        self.package("windows-x64")
        with self.assertRaises(FileNotFoundError):
            publisher.verify_artifacts(self.destination, TAG, COMMIT)
        self.package("linux-x64")
        assets = publisher.verify_artifacts(self.destination, TAG, COMMIT)
        self.assertEqual(len(assets), 3)
        self.assertEqual(len(assets[-1].read_text().splitlines()), 2)
        with self.assertRaisesRegex(ValueError, "Source identity"):
            publisher.verify_artifacts(self.destination, TAG, "b" * 40)
        with assets[0].open("ab") as output:
            output.write(b"damaged")
        with self.assertRaisesRegex(ValueError, "checksum"):
            publisher.verify_artifacts(self.destination, TAG, COMMIT)

    def test_missing_binary_fails_before_archiving(self):
        (self.build / "theweb-enhanced.exe").unlink()
        with self.assertRaises(FileNotFoundError):
            self.package("windows-x64")
        self.assertFalse(self.destination.exists())

    def test_debug_binary_is_rejected(self):
        path = self.build / "theweb.exe"
        content = bytearray(path.read_bytes())
        struct.pack_into("<II", content, 0x80 + 12, 1, 1)
        path.write_bytes(content)
        with self.assertRaisesRegex(AssertionError, "COFF symbols"):
            self.package("windows-x64")

    def test_pe_release_metadata_is_accepted_in_pe32_and_pe32plus(self):
        entries = [(12, struct.pack("<IIIII", 0, 1, 0, 0, 1)), (16, b""),
                   (16, struct.pack("<I", 32) + b"h" * 32), (20, struct.pack("<I", 1))]
        path = self.build / "theweb.exe"
        for pe32 in (False, True):
            with self.subTest(pe32=pe32):
                path.write_bytes(pe_image(entries, pe32))
                result = verifier.inspect(path)
                self.assertFalse(result["debug_information"])
                self.assertEqual([entry["type"] for entry in result["pe_debug_metadata"]], [12, 16, 16, 20])
        self.package("windows-x64")

    def test_pe_symbols_pdb_references_and_unknown_records_are_rejected(self):
        path = self.build / "theweb.exe"
        for kind in (1, 2, 4, 17, 19, 99):
            with self.subTest(kind=kind):
                path.write_bytes(pe_image([(16, b""), (kind, b"RSDSprivate.pdb\0")]))
                with self.assertRaisesRegex(AssertionError, f"type {kind}"):
                    verifier.inspect(path)

    def test_pe_pogo_coff_group_layout_is_accepted(self):
        path = self.build / "theweb.exe"
        groups = bytearray()
        for rva, size, name in ((0x1000, 0x100, b".text$mn"), (0x2000, 0x20, b".rdata$zzz")):
            groups.extend(struct.pack("<II", rva, size) + name + b"\0")
            groups.extend(b"\0" * (-len(groups) % 4))
        for pe32 in (False, True):
            for signature in (0, 0x4c544347, 0x50474900, 0x50474f00, 0x50475500, 0x5350474f):
                with self.subTest(pe32=pe32, signature=signature):
                    payload = struct.pack("<I", signature) + groups
                    path.write_bytes(pe_image([(13, payload), (16, b"")], pe32))
                    metadata = verifier.inspect(path)["pe_debug_metadata"]
                    self.assertEqual(metadata[0]["name"], "POGO")
                    self.assertEqual(metadata[0]["coff_groups"], 2)
        self.package("windows-x64")
        path.write_bytes(pe_image([(13, payload), (2, b"RSDSprivate.pdb\0")]))
        with self.assertRaisesRegex(AssertionError, "CODEVIEW/PDB"):
            verifier.inspect(path)

    def test_malformed_pe_pogo_records_are_rejected(self):
        path = self.build / "theweb.exe"
        for payload in (b"RSDSprivate.pdb\0", b"GC", b"GCTLshort", b"GCTL" + struct.pack("<II", 0, 1) + b"no-nul",
                        b"GCTL" + struct.pack("<II", 0, 1) + b"x\0"):
            with self.subTest(payload=payload):
                path.write_bytes(pe_image([(13, payload)]))
                with self.assertRaisesRegex(AssertionError, "POGO"):
                    verifier.inspect(path)

    def test_malformed_pe_debug_directory_and_payload_are_rejected(self):
        path = self.build / "theweb.exe"
        for field, value, message in ((0x98 + 112 + 6 * 8 + 4, 29, "malformed"),
                                      (0x98 + 112 + 6 * 8, 0xffff0000, "unmapped"),
                                      (0x200 + 16, 0x10000, "truncated"),
                                      (0x200 + 24, 0, "truncated")):
            with self.subTest(field=field):
                image = pe_image([(20, struct.pack("<I", 1))])
                struct.pack_into("<I", image, field, value)
                path.write_bytes(image)
                with self.assertRaisesRegex(AssertionError, message):
                    verifier.inspect(path)

    def test_pe_debug_sections_are_still_rejected(self):
        path = self.build / "theweb.exe"
        image = pe_image()
        image[0x98 + 0xf0:0x98 + 0xf0 + 8] = b".debug\0\0"
        path.write_bytes(image)
        with self.assertRaisesRegex(AssertionError, "debug sections"):
            verifier.inspect(path)

    def test_smoke_failure_prevents_package(self):
        with patch.object(packager.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "game")):
            with self.assertRaises(subprocess.CalledProcessError):
                packager.package(self.root, self.build, self.destination, "windows-x64", TAG, COMMIT)
        self.assertFalse(self.destination.exists())

    def test_unsafe_tags_and_invalid_commits(self):
        for tag in ("../v1", "v1/subdir", "v1\nother", "--help", "vhello"):
            with self.subTest(tag=tag), self.assertRaises(ValueError):
                packager.validate_identity(tag, COMMIT)
        with self.assertRaises(ValueError):
            packager.validate_identity(TAG, "short")

    def test_publication_creates_prerelease_with_verified_tag(self):
        self.package("windows-x64")
        self.package("linux-x64")
        missing = subprocess.CompletedProcess(["gh"], 1, stdout="", stderr="release not found")
        with patch.object(publisher.subprocess, "run", side_effect=[missing, subprocess.CompletedProcess([], 0)]) as gh:
            publisher.publish(TAG, COMMIT, self.destination)
        command = gh.call_args_list[1].args[0]
        self.assertEqual(command[:4], ["gh", "release", "create", TAG])
        self.assertIn("--verify-tag", command)
        self.assertIn("--prerelease", command)
        self.assertIn("--notes-file", command)

    def test_publication_preserves_notes_and_completes_draft(self):
        self.package("windows-x64")
        self.package("linux-x64")
        draft = subprocess.CompletedProcess([], 0, stdout=json.dumps({"isDraft": True, "isImmutable": False}))
        with patch.object(publisher.subprocess, "run", side_effect=[draft, subprocess.CompletedProcess([], 0),
                                                                 subprocess.CompletedProcess([], 0)]) as gh:
            publisher.publish(TAG, COMMIT, self.destination)
        commands = [call.args[0] for call in gh.call_args_list]
        self.assertEqual(commands[1][:4], ["gh", "release", "upload", TAG])
        self.assertIn("--clobber", commands[1])
        self.assertEqual(commands[2], ["gh", "release", "edit", TAG, "--draft=false"])

    def test_immutable_release_and_api_errors_stop_publication(self):
        self.package("windows-x64")
        self.package("linux-x64")
        immutable = subprocess.CompletedProcess([], 0, stdout=json.dumps({"isDraft": False, "isImmutable": True}))
        with patch.object(publisher.subprocess, "run", return_value=immutable) as gh:
            with self.assertRaisesRegex(SystemExit, "immutable"):
                publisher.publish(TAG, COMMIT, self.destination)
        self.assertEqual(gh.call_count, 1)
        forbidden = subprocess.CompletedProcess(["gh"], 1, stdout="", stderr="HTTP 403 forbidden")
        with patch.object(publisher.subprocess, "run", return_value=forbidden) as gh:
            with self.assertRaises(subprocess.CalledProcessError):
                publisher.publish(TAG, COMMIT, self.destination)
        self.assertEqual(gh.call_count, 1)


if __name__ == "__main__":
    unittest.main()
