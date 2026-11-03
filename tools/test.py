"""Configure, build and run the tests without game data or desktop dependencies."""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--generator", help="CMake generator, for example MinGW Makefiles")
    parser.add_argument("--build-dir", type=Path, help="Use a separate test build directory")
    args = parser.parse_args()
    root = Path(__file__).resolve().parent.parent
    build = args.build_dir.resolve() if args.build_dir else root / "build" / "tests"
    configure = ["cmake", "-S", str(root), "-B", str(build),
                 "-DWEB_BUILD_GAMES=OFF", "-DBUILD_TESTING=ON", "-DCMAKE_BUILD_TYPE=Release",
                 f"-DPython3_EXECUTABLE={sys.executable}"]
    if args.generator:
        configure += ["-G", args.generator]
    commands = [configure,
                ["cmake", "--build", str(build), "--config", "Release", "--parallel",
                 os.environ.get("CMAKE_BUILD_PARALLEL_LEVEL", "2")],
                ["ctest", "--test-dir", str(build), "-C", "Release", "--output-on-failure", "--no-tests=error"],
                [sys.executable, str(root / "tools/test-release.py")]]
    for command in commands:
        print("+", subprocess.list2cmdline(command), flush=True)
        subprocess.run(command, cwd=root, check=True)


if __name__ == "__main__":
    try:
        main()
    except FileNotFoundError as error:
        sys.exit(f"Missing build tool: {error.filename}. See README prerequisites.")
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
