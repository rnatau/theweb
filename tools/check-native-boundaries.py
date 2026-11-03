"""Enforce the portable core's dependency direction, including header-only code."""
from pathlib import Path
import argparse
import re

ROOT = Path(__file__).resolve().parent.parent
NATIVE = ROOT / "native"
STANDARD_HEADERS = set("""
algorithm any array atomic barrier bit bitset cassert cctype cerrno cfenv cfloat
charconv chrono cinttypes ciso646 climits clocale cmath codecvt compare complex
concepts condition_variable coroutine csetjmp csignal cstdarg cstddef cstdint
cstdio cstdlib cstring ctime cuchar cwchar cwctype deque exception execution
filesystem forward_list fstream functional future initializer_list iomanip ios
iosfwd iostream istream iterator latch limits list locale map memory memory_resource
mutex new numbers numeric optional ostream queue random ranges ratio regex
scoped_allocator semaphore set shared_mutex source_location span sstream stack
stdexcept stop_token streambuf string string_view strstream syncstream system_error
thread tuple type_traits typeindex typeinfo unordered_map unordered_set utility
valarray variant vector version
assert.h ctype.h errno.h fenv.h float.h inttypes.h iso646.h limits.h locale.h math.h
setjmp.h signal.h stdarg.h stddef.h stdint.h stdio.h stdlib.h string.h time.h uchar.h
wchar.h wctype.h
""".split())


def check(edition="both"):
    errors = []
    files = [p for p in NATIVE.rglob("*")
             if p.suffix in {".cpp", ".hpp", ".inc"}
             and p.relative_to(NATIVE).parts[0] not in {"platform", "tests"}]
    if edition != "both":
        files = [p for p in files if p.relative_to(NATIVE).parts[0] != "editions"
                 or p.relative_to(NATIVE).parts[1] == edition]
    for path in files:
        content = path.read_text(encoding="utf-8")
        for kind, header in re.findall(r'^\s*#\s*include\s*([<"])([^>"\n]+)[>"]', content, re.M):
            if kind == "<":
                if header not in STANDARD_HEADERS:
                    errors.append(f"{path.relative_to(ROOT)}: nonstandard core header <{header}>")
            else:
                target = (path.parent / header).resolve()
                if not target.is_relative_to(NATIVE) or target.relative_to(NATIVE).parts[0] in {"platform", "tests"}:
                    errors.append(f"{path.relative_to(ROOT)}: forbidden dependency {header}")
                elif not target.is_file():
                    errors.append(f"{path.relative_to(ROOT)}: missing dependency {header}")
                elif target.parent == NATIVE / "compatibility" and path.parent != NATIVE / "compatibility" and target.name != "physics_metadata.hpp":
                    errors.append(f"{path.relative_to(ROOT)}: development snapshot dependency {header}")
                elif target.relative_to(NATIVE).parts[0] == "editions":
                    owner = path.relative_to(NATIVE).parts
                    destination = target.relative_to(NATIVE).parts[1]
                    if owner[0] != "editions" or owner[1] != destination:
                        errors.append(f"{path.relative_to(ROOT)}: forbidden edition dependency {header}")
        if re.search(r'^\s*#\s*(?:if|ifdef|ifndef|elif).*\b(?:_WIN32|WIN32|_WIN64|__linux__|__APPLE__)\b', content, re.M):
            errors.append(f"{path.relative_to(ROOT)}: platform conditional in core")
    # The shared Windows shell may include OS headers, but must also stay free
    # of edition implementations. It selects them only through the contract.
    if edition != "core":
        for path in (NATIVE / "platform").rglob("*"):
            if path.suffix not in {".cpp", ".hpp", ".inc"}:
                continue
            for header in re.findall(r'^\s*#\s*include\s*"([^"\n]+)"', path.read_text(encoding="utf-8"), re.M):
                if (path.parent / header).resolve().is_relative_to(NATIVE / "editions"):
                    errors.append(f"{path.relative_to(ROOT)}: forbidden edition dependency {header}")
    listed = (NATIVE / "core-sources.txt").read_text().splitlines()
    actual = {p.relative_to(NATIVE).as_posix() for p in files if p.suffix == ".cpp" and p.relative_to(NATIVE).parts[0] != "editions"}
    if set(listed) != actual or len(listed) != len(set(listed)):
        errors.append(f"Core source manifest differs: missing={sorted(actual - set(listed))}, extra={sorted(set(listed) - actual)}")
    editions = ("classic", "enhanced") if edition == "both" else () if edition == "core" else (edition,)
    for selected in editions:
        entries = (NATIVE / "editions" / selected / "sources.txt").read_text().splitlines()
        expected = {p.relative_to(NATIVE).as_posix() for p in (NATIVE / "editions" / selected).rglob("*.cpp")}
        if set(entries) != expected or len(entries) != len(set(entries)):
            errors.append(f"{selected} source manifest differs from its implementation files")
        if any(not entry.startswith(f"editions/{selected}/") for entry in entries):
            errors.append(f"{selected} manifest contains a source outside its edition")
    if errors:
        raise SystemExit("\n".join(errors))
    print(f"PASS: {edition} boundary scope, {len(files)} portable files, {len(listed)} core translation units; standard headers only, no platform/test dependencies")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--edition", choices=["both", "core", "classic", "enhanced"], default="both")
    check(parser.parse_args().edition)
