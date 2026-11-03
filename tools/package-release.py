"""Create a media-free release archive from two explicitly named shipping games."""
import argparse
import hashlib
import importlib.util
import io
import json
import re
from pathlib import Path
import subprocess
import tarfile
import tempfile
import zipfile


ROOT = Path(__file__).resolve().parent.parent


def validate_identity(tag, commit):
    if not re.fullmatch(r"v[0-9][A-Za-z0-9._+-]{0,99}", tag):
        raise ValueError("Expected a version tag beginning with v and a digit, without slashes or spaces")
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Expected the full source commit SHA")


def sha256(content):
    return hashlib.sha256(content).hexdigest()


def package(root, build, destination, platform, tag, commit):
    validate_identity(tag, commit)
    if platform not in {"windows-x64", "linux-x64"}:
        raise ValueError("Unsupported release platform")
    verifier_spec = importlib.util.spec_from_file_location(
        "release_binary_verifier", root / "tools" / "verify-release-binaries.py")
    verifier = importlib.util.module_from_spec(verifier_spec)
    verifier_spec.loader.exec_module(verifier)
    windows = platform == "windows-x64"
    names = [name + (".exe" if windows else "") for name in ("theweb", "theweb-enhanced")]
    files, versions = {}, {}
    # Copy only these named binaries. Never walk a build tree or copy data/DLLs.
    with tempfile.TemporaryDirectory(prefix="theweb-release-") as temporary:
        stage = Path(temporary)
        for name in names:
            source = build / name
            result = verifier.inspect(source)
            if result["format"] != ("PE" if windows else "ELF"):
                raise ValueError(f"Wrong executable format for {platform}: {name}")
            files[name] = source.read_bytes()
            executable = stage / name
            executable.write_bytes(files[name])
            executable.chmod(0o755)
            # Launch from a clean folder with no runtime DLLs, game files or saves.
            version = subprocess.run([str(executable), "--version"], cwd=stage,
                                     check=True, capture_output=True, text=True, timeout=30).stdout.strip()
            if not version:
                raise ValueError(f"No version output from {name}")
            subprocess.run([str(executable), "--help"], cwd=stage, check=True,
                           capture_output=True, text=True, timeout=30)
            versions[name] = version
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
    archive = destination / (name + (".zip" if windows else ".tar.gz"))
    if windows:
        with zipfile.ZipFile(archive, "w", compression=zipfile.ZIP_DEFLATED) as output:
            for filename, content in sorted(files.items()):
                output.writestr(f"{name}/{filename}", content)
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
    parser.add_argument("--platform", choices=["windows-x64", "linux-x64"], required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()
    print(package(ROOT, args.build_dir.resolve(), args.output_dir.resolve(),
                  args.platform, args.tag, args.commit))


if __name__ == "__main__":
    main()
