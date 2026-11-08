"""Build the two shared release libraries and retain their exact source archives."""
import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
import urllib.request


# Pin real SDL2 rather than Homebrew's SDL2-to-SDL3 compatibility layer.
SOURCES = (
    ("https://www.libsdl.org/release/SDL2-2.32.10.tar.gz",
     "5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165"),
    ("https://www.mpg123.de/download/mpg123-1.33.7.tar.bz2",
     "31d0e35a4ca567ec9b5ebda6c3062bb4435d6d3eacd6ef0d95cadd7854dc03ee"),
)


def build(directory):
    if sys.platform != "darwin":
        raise SystemExit("Build macOS release dependencies on a Mac")
    directory = directory.resolve()
    prefix = directory / "install"
    sources = prefix / "sources"
    sources.mkdir(parents=True, exist_ok=True)
    for url, digest in SOURCES:
        archive = sources / url.rsplit("/", 1)[1]
        with urllib.request.urlopen(url, timeout=120) as response:
            content = response.read()
        if hashlib.sha256(content).hexdigest() != digest:
            raise ValueError(f"Source checksum mismatch: {url}")
        archive.write_bytes(content)
        with tarfile.open(archive) as source:
            source.extractall(directory, filter="data")
    environment = {**os.environ, "MACOSX_DEPLOYMENT_TARGET": "15.0"}
    run = lambda *args, **kwargs: subprocess.run(list(args), check=True, env=environment, **kwargs)
    run("cmake", "-S", str(directory / "SDL2-2.32.10"), "-B", str(directory / "sdl-build"),
        "-DCMAKE_BUILD_TYPE=Release", f"-DCMAKE_INSTALL_PREFIX={prefix}",
        "-DSDL_SHARED=ON", "-DSDL_STATIC=OFF", "-DSDL_TEST=OFF", "-DSDL_TESTS=OFF")
    run("cmake", "--build", str(directory / "sdl-build"), "--parallel", "2")
    run("cmake", "--install", str(directory / "sdl-build"))
    run(str(directory / "mpg123-1.33.7/configure"), f"--prefix={prefix}",
        "--enable-shared", "--disable-static", "--with-default-audio=dummy",
        "--disable-network", cwd=directory / "mpg123-1.33.7")
    run("make", "-j2", cwd=directory / "mpg123-1.33.7")
    run("make", "install", cwd=directory / "mpg123-1.33.7")
    notices = prefix / "licenses"
    notices.mkdir(exist_ok=True)
    shutil.copyfile(directory / "SDL2-2.32.10/LICENSE.txt", notices / "SDL2-LICENSE.txt")
    shutil.copyfile(directory / "mpg123-1.33.7/COPYING", notices / "mpg123-COPYING")
    shutil.copyfile(Path(__file__), sources / "build-macos-dependencies.py")
    (sources / "BUILD.txt").write_text(
        "SDL2 and libmpg123 are dynamically linked, unmodified upstream libraries.\n"
        "Exact source archives and the build script are included here.\n"
        "On macOS with Xcode command line tools, CMake and Python 3.12+, run:\n"
        "python3 build-macos-dependencies.py --build-dir dependencies\n"
        "Replace libraries in Contents/MacOS of either app to use rebuilt versions.\n"
        "After replacing libraries, ad-hoc sign the app again:\n"
        "codesign --force --deep --sign - '/path/to/The Web Classic.app'\n"
        "No hardened runtime or library validation is enabled.\n", encoding="utf-8")
    return prefix


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    print(build(parser.parse_args().build_dir))
