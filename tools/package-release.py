"""Create a media-free release archive from two explicitly named shipping games."""
import argparse
import hashlib
import importlib.util
import io
import json
import os
import re
import plistlib
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parent.parent
PLATFORMS = {"windows-x64": ".zip", "linux-x64": ".tar.gz",
             "macos-arm64": ".zip", "macos-x64": ".zip"}
MACOS_APPS = {"theweb": "The Web Classic.app", "theweb-enhanced": "The Web Enhanced.app"}


def validate_identity(tag, commit):
    if not re.fullmatch(r"v[0-9][A-Za-z0-9._+-]{0,99}", tag):
        raise ValueError("Expected a version tag beginning with v and a digit, without slashes or spaces")
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Expected the full source commit SHA")


def sha256(content):
    return hashlib.sha256(content).hexdigest()


def macos_bundle(root, source, stage, dependencies, tag, commit, verifier, architecture):
    app = stage / MACOS_APPS[source.name]
    executable = app / "Contents/MacOS" / source.name
    executable.parent.mkdir(parents=True)
    (app / "Contents/Resources").mkdir()
    shutil.copyfile(source, executable)
    executable.chmod(0o755)
    edition = "classic" if source.name == "theweb" else "enhanced"
    bundle_version = ".".join(str(int(part)) for part in
                              re.match(r"v(\d+(?:\.\d+){0,2})", tag).group(1).split("."))
    with (app / "Contents/Info.plist").open("wb") as output:
        plistlib.dump({"CFBundleExecutable": source.name, "CFBundleIdentifier": f"org.theweb.{edition}",
                      "CFBundleName": app.stem, "CFBundleDisplayName": app.stem,
                      "CFBundlePackageType": "APPL", "CFBundleVersion": bundle_version,
                      "CFBundleShortVersionString": bundle_version,
                      "LSMinimumSystemVersion": "15.0", "NSHighResolutionCapable": True,
                      "SDL_FILESYSTEM_BASE_DIR_TYPE": "parent",
                      "TheWebSourceTag": tag, "TheWebSourceCommit": commit}, output)
    subprocess.run(["cmake", f"-DWEB_APP={app}", "-P", str(root / "cmake/FixMacOSBundle.cmake")], check=True)
    # BundleUtilities copies only linked libraries, never the build's game data.
    binaries = [path for path in app.rglob("*") if path.is_file() and not path.is_symlink()
                and path.read_bytes()[:4] == b"\xcf\xfa\xed\xfe"]
    for binary in binaries:
        subprocess.run(["strip", "-S", "-x", str(binary)], check=True)
        result = verifier.inspect(binary)
        if result.get("architecture") != architecture:
            raise ValueError(f"Wrong Mach-O architecture: {binary.name}")
        if binary != executable:
            subprocess.run(["codesign", "--force", "--sign", "-", str(binary)], check=True)
    licenses = app / "Contents/Resources/licenses"
    shutil.copytree(dependencies / "licenses", licenses)
    subprocess.run(["codesign", "--force", "--sign", "-", str(app)], check=True)
    subprocess.run(["codesign", "--verify", "--deep", "--strict", str(app)], check=True)
    return executable


def package(root, build, destination, platform, tag, commit, macos_dependencies=None):
    validate_identity(tag, commit)
    if platform not in PLATFORMS:
        raise ValueError("Unsupported release platform")
    macos = platform.startswith("macos-")
    if macos and macos_dependencies is None:
        raise ValueError("macOS packaging requires --macos-dependencies with libraries, licenses and sources")
    verifier_spec = importlib.util.spec_from_file_location(
        "release_binary_verifier", root / "tools" / "verify-release-binaries.py")
    verifier = importlib.util.module_from_spec(verifier_spec)
    verifier_spec.loader.exec_module(verifier)
    windows = platform == "windows-x64"
    names = [name + (".exe" if windows else "") for name in ("theweb", "theweb-enhanced")]
    files, versions, modes, links = {}, {}, {}, {}
    executables = set()
    # Stage only named shipping binaries, plus linked macOS libraries and notices.
    with tempfile.TemporaryDirectory(prefix="theweb-release-") as temporary:
        stage = Path(temporary)
        for name in names:
            source = build / name
            result = verifier.inspect(source)
            if result["format"] != ("Mach-O" if macos else "PE" if windows else "ELF"):
                raise ValueError(f"Wrong executable format for {platform}: {name}")
            if macos:
                if result.get("architecture") != platform.removeprefix("macos-"):
                    raise ValueError(f"Wrong executable architecture for {platform}: {name}")
                executable = macos_bundle(root, source, stage, macos_dependencies, tag, commit,
                                          verifier, platform.removeprefix("macos-"))
            else:
                executable = stage / name
                shutil.copyfile(source, executable)
                executable.chmod(0o755)
            # Launch from a clean folder with no runtime DLLs, game files or saves.
            version = subprocess.run([str(executable), "--version"], cwd=stage,
                                     check=True, capture_output=True, text=True, timeout=30).stdout.strip()
            if not version:
                raise ValueError(f"No version output from {name}")
            subprocess.run([str(executable), "--help"], cwd=stage, check=True,
                           capture_output=True, text=True, timeout=30)
            versions[name] = version
            executables.add(executable)
        if macos:
            # Ship exact corresponding library sources and reproduction instructions.
            shutil.copytree(macos_dependencies / "sources", stage / "third-party-sources")
        for path in sorted(stage.rglob("*")):
            if path.is_symlink() or path.is_file():
                relative = path.relative_to(stage).as_posix()
                files[relative] = path.read_bytes()
                if path.is_symlink():
                    # Preserve the layout sealed by codesign, including library aliases.
                    path.resolve().relative_to(stage.resolve())
                    links[relative] = os.readlink(path).encode("utf-8")
                modes[relative] = 0o755 if path in executables or path.stat().st_mode & 0o111 else 0o644
    for name in ("README.md", "LICENSE"):
        files[name] = (root / name).read_bytes()
    files["build-info.json"] = (json.dumps({
        "tag": tag, "commit": commit, "platform": platform,
        "configuration": "Release", "versions": versions,
        "sha256": {name: sha256(content) for name, content in files.items()},
    }, indent=2) + "\n").encode("utf-8")
    files["SHA256SUMS"] = "".join(
        f"{sha256(content)}  {name}\n" for name, content in sorted(files.items())).encode("utf-8")
    name = f"TheWeb-{tag}-{platform}"
    destination.mkdir(parents=True, exist_ok=True)
    archive = destination / (name + PLATFORMS[platform])
    if PLATFORMS[platform] == ".zip":
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as output:
            for filename, content in sorted(files.items()):
                info = zipfile.ZipInfo(f"{name}/{filename}")
                info.create_system = 3
                info.external_attr = (0o120777 if filename in links else
                                      0o100000 | modes.get(filename, 0o644)) << 16
                output.writestr(info, links.get(filename, content), compress_type=zipfile.ZIP_DEFLATED)
    else:
        with tarfile.open(archive, "w:gz") as output:
            for filename, content in sorted(files.items()):
                info = tarfile.TarInfo(f"{name}/{filename}")
                info.size = len(content)
                info.mode = 0o755 if filename in names else 0o644
                output.addfile(info, io.BytesIO(content))
    archive.with_name(archive.name + ".sha256").write_text(
        f"{sha256(archive.read_bytes())}  {archive.name}\n", encoding="utf-8")
    return archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, default=ROOT / "dist")
    parser.add_argument("--platform", choices=PLATFORMS, required=True)
    parser.add_argument("--macos-dependencies", type=Path, help="Prefix from build-macos-dependencies.py")
    parser.add_argument("--tag", required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    print(package(ROOT, args.build_dir.resolve(), args.output_dir.resolve(),
                  args.platform, args.tag, args.commit,
                  args.macos_dependencies.resolve() if args.macos_dependencies else None))


if __name__ == "__main__":
    main()
