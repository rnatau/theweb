"""Verify both platform artifacts, then attach them to their existing source tag."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tarfile
import zipfile


def verify_artifacts(directory, tag, commit):
    checksums = []
    archives = []
    for platform, extension in (("windows-x64", ".zip"), ("linux-x64", ".tar.gz")):
        name = f"TheWeb-{tag}-{platform}"
        archive = directory / (name + extension)
        expected = hashlib.sha256(archive.read_bytes()).hexdigest() + f"  {archive.name}\n"
        if archive.with_name(archive.name + ".sha256").read_text(encoding="utf-8") != expected:
            raise ValueError(f"Archive checksum mismatch: {archive.name}")
        if extension == ".zip":
            with zipfile.ZipFile(archive) as source:
                metadata = json.loads(source.read(f"{name}/build-info.json"))
        else:
            with tarfile.open(archive) as source:
                metadata = json.load(source.extractfile(f"{name}/build-info.json"))
        if any(metadata[key] != value for key, value in
               (("tag", tag), ("commit", commit), ("platform", platform), ("configuration", "Release"))):
            raise ValueError(f"Source identity mismatch: {archive.name}")
        checksums.append(expected)
        archives.append(archive)
    sums = directory / "SHA256SUMS"
    sums.write_text("".join(checksums), encoding="utf-8")
    return [*archives, sums]


def publish(tag, commit, directory):
    assets = verify_artifacts(directory, tag, commit)
    notes = directory / "release-notes.md"
    notes.write_text(
        f"Classic and Enhanced, built from `{commit}` (tag `{tag}`).\n\n"
        "Download the Windows x64 ZIP or Linux x64 tar.gz and extract it. "
        "Both editions are included. These downloads contain no game data or music. "
        "Supply your own complete `PC_DAT` folder as `data/PC_DAT` beside the games; "
        "see the included README for setup and optional music.\n\n"
        "Windows 10/11 x64: the Visual C++ runtime is linked statically. "
        "Linux x64: built on Ubuntu 22.04; install SDL2 and mpg123 runtime libraries "
        "(`sudo apt-get install libsdl2-2.0-0 libmpg123-0` on Debian/Ubuntu).\n\n"
        "`SHA256SUMS` verifies the downloads. Each archive includes per-file checksums "
        "and `build-info.json` with source and edition versions.\n", encoding="utf-8")
    gh = lambda *args, **kwargs: subprocess.run(["gh", "release", *args], **kwargs)
    existing = gh("view", tag, "--json", "isDraft,isImmutable", capture_output=True, text=True)
    if existing.returncode == 0:
        state = json.loads(existing.stdout)
        if state["isImmutable"]:
            raise SystemExit("This release is immutable. Create a new version tag for new binaries.")
        gh("upload", tag, *map(str, assets), "--clobber", check=True)
        if state["isDraft"]:
            gh("edit", tag, "--draft=false", check=True)
    else:
        if "release not found" not in existing.stderr.lower():
            raise subprocess.CalledProcessError(existing.returncode, existing.args,
                                                existing.stdout, existing.stderr)
        options = ["--prerelease"] if "-" in tag else []
        gh("create", tag, *map(str, assets), "--verify-tag", "--title", f"The Web {tag}",
           "--notes-file", str(notes), *options, check=True)


if __name__ == "__main__":
    publish(os.environ["RELEASE_TAG"], os.environ["RELEASE_COMMIT"], Path("dist"))
