# Pro Pinball: The Web — native reconstruction

A C++20 reconstruction for modern Windows, Linux and macOS, with **Classic 1.52p**
(`theweb`) and **Enhanced 1.60a** (`theweb-enhanced`).

**This effort relied HEAVILY on OpenAI Codex** for reconstruction, implementation,
testing and documentation, guided by a human developer. This is an unofficial
project, unaffiliated with the original game's creators or rights holders.

Only source code, build files and tests are included. There is no original game
executable, artwork, font artwork, sound, music, disassembly or analysis archive.
You must supply your own legally obtained game data to play. Compiling and running
the tests requires no original game files.

## Classic

Reconstructed Windows 1.50P rules and integer physics, with the supplied 1.52P
caption. Includes one to four players, all missions/modes, original menus,
six cameras, DMD, sound effects and optional MP3 music.

- Runs on modern Windows/Linux/macOS without a CD or original executable.
- Aspect-preserving fullscreen/windowed scaling and crisp DMD dots.
- Separate edition saves, atomic writes and damaged-file protection.
- Windows Raw Input keyboard handling.
- Safety guards and Space Station exit-artwork cleanup.

Classic otherwise preserves original behavior and quality settings.

## Enhanced

- DirectX 11 GPU rendering on Windows, with GDI fallback.
- Frame-synchronized high-refresh play; validated at 175 Hz on Windows.
- Timestamped keyboard/controller input applied within physics ticks.
- Remappable keyboard, Xbox and generic Windows gamepad controls.
- Event-driven WASAPI audio, independent of rendering.
- Pause-menu resume and integrated settings menus.
- Maximum graphics/effects quality: 1024×768, 44.1 kHz stereo, 16-bit.
- Antialiased ball/flipper edges and partial ball occlusion; the DMD stays crisp.
- Directional nudging with persistent ball displacement.
- A 10% narrower lower-flipper gap.
- Hold Start for one second to launch any waiting ball and drain into standby,
  with the pre-game DMD shown immediately. Press Start again after draining.
- Missions don't get interrupted by other events
- Pre-buy-in high scores are still stored
- Original physics, rule, display and stereo bug corrections.

High-refresh rendering preserves 60 Hz rules/timers and original DMD cadence,
without interpolation.

## Build

Run commands from this repository's root. No game data is needed to compile.

### Download release binaries

GitHub Releases provide a Windows x64 ZIP and a Linux x64 tar.gz, each containing
Classic and Enhanced. Extract the archive, then supply `data/PC_DAT` beside the
executables as described below. Game data and music are never included.
Windows release downloads link the Visual C++ runtime statically. Linux downloads
are built on Ubuntu 22.04 and require SDL2 and mpg123 runtime libraries
(`sudo apt-get install libsdl2-2.0-0 libmpg123-0` on Debian/Ubuntu).
Each release includes `SHA256SUMS`; each archive includes per-file checksums and
`build-info.json` identifying the source commit and edition versions.

### Windows

Prerequisites: 64-bit Windows 10/11, **Visual Studio 2022 Build Tools** with
**Desktop development with C++** and a Windows SDK, **CMake 3.20 or newer** on
PATH, and **Python 3.9 or newer** on PATH for tests and architecture checks.
Open **Developer PowerShell for VS 2022**, then:

```powershell
cmake -S . -B build/game -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=OFF
cmake --build build/game --config Release --parallel 2
```

The executables are `build/game/Release/theweb.exe` and
`build/game/Release/theweb-enhanced.exe`. Windows Media Foundation must be
available for optional MP3 music (Windows N editions may need the Media Feature
Pack). Keep the Visual C++ runtime required by your compiler installed.

MinGW-w64 is also supported: put its C++20-capable compiler and `mingw32-make`
on PATH, use `-G "MinGW Makefiles"` in a **different build folder**, and omit
`-A x64`. Its single-configuration executables are directly in that build folder.
Keep any compiler runtime DLLs required by your chosen toolchain alongside them
or on PATH.

### Linux (Debian/Ubuntu)

Prerequisites: a C++20 compiler (GCC 11 or newer), CMake 3.20 or newer, Make,
SDL2 and mpg123 development packages. Install the dependencies, then build:

```sh
sudo apt-get update
sudo apt-get install build-essential cmake libsdl2-dev libmpg123-dev python3
cmake -S . -B build/game -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/game --parallel 2
```

The executables are `build/game/theweb` and `build/game/theweb-enhanced`.
SDL2 and mpg123 runtime libraries are required even when music is absent.

### macOS (Apple Silicon or Intel)

Install the Xcode command line tools (`xcode-select --install`) and
[Homebrew](https://brew.sh), then run:

```sh
brew install cmake sdl2 mpg123 python
cmake -S . -B build/game -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF
cmake --build build/game --parallel 2
./build/game/theweb --help
./build/game/theweb-enhanced --help
```

Both editions use SDL2 for graphics, keyboard/controller input and audio on
macOS. These are native command-line executables, built for your Mac's
architecture; keep the Homebrew SDL2 and mpg123 libraries installed.
Supply the original game data as described below, then launch either executable
from Terminal. Option+Enter toggles fullscreen. Saves are stored separately per
edition under `~/Library/Application Support/theweb/`; `--save-dir PATH`
overrides this location. No game data is required to build or run the synthetic
test suite (`python3 tools/test.py`).

## Supply game data and music

Copy the **complete `PC_DAT` directory** from your original game media or
installation, preserving its subdirectories and filenames. Put it in a local
`data` folder **beside the executable**. Do not rename it or flatten its contents.
It contains the graphics, DMD fonts/animations, sound samples, collision maps and
table data needed by both editions. No original EXE, installer or DirectX files
are required. On Linux, preserve the original uppercase names; filenames are
case-sensitive.

Use this layout (Windows build shown):

```text
build/game/Release/
  theweb.exe
  theweb-enhanced.exe
  data/
    PC_DAT/
      COLLIDE/
      MATRIX/
      FRONTEND/
      ...all other original PC_DAT contents...
    MUSIC/                 optional
      Track02.mp3
      Track03.mp3
      ...
      Track24.mp3
```

For macOS, Linux or MinGW, place the same `data` directory directly in `build/game/`
(or the custom build folder). Adjacent `PC_DAT/` and parent `data/PC_DAT/` are
also accepted. Data is found relative to the executable, independently of the
terminal's working directory.

**Music is optional; the game works without it.** Encode audio tracks you are
entitled to use as MP3, one file per physical CD audio track, keeping the disc's
track numbers: **`Track02.mp3` through `Track24.mp3`**, with exactly that spelling
and two-digit numbers. Track 01 is the data track; do not create `Track01.mp3` or
renumber the first audio track to 01. Renaming a WAV file to `.mp3` does not
convert it. Put the files in `data/MUSIC/` as shown, or in `data/PC_DAT/MUSIC/`.
Use one location; an existing inner `MUSIC` directory takes precedence. Missing
tracks have no replacement music. No ripping or conversion tool is required by
the build.

Do not commit or upload supplied data, music or packages containing them.
They are excluded by `.gitignore`.

## Play

After adding data, launch either executable. For example:

```powershell
.\build\game\Release\theweb.exe
.\build\game\Release\theweb-enhanced.exe
```

On Linux or macOS: `./build/game/theweb` or `./build/game/theweb-enhanced`.
Any key skips the intro. Defaults:

| Key | Action |
| --- | --- |
| S | Start / add player; Enhanced: hold one second to restart |
| Enter | Launch; skip bonus |
| C | Buy in at the continue prompt |
| F / J | Left / right flippers (J also controls the upper flipper) |
| Space / G / H | Up / side nudges |
| P | Pause / resume |
| V | Change camera |
| Alt+Enter | Toggle fullscreen |
| Escape | Classic: return home; Enhanced: open pause menu |
| F2 / F4 | Classic settings / menu overlay |

Enhanced settings and controller bindings are in OPTIONS. Use `--windowed`,
`--fullscreen`, `--mute`, `--save-dir PATH`, `--no-save`, `--help` or `--version`
as needed. Saves use separate `classic` and `enhanced` subfolders under
`saves/` beside the executables on Windows. Linux uses `$XDG_DATA_HOME/theweb/`
or `~/.local/share/theweb/`. `--save-dir PATH` selects an explicit parent.
For record initials, use F/J to select letters, Enter/S to confirm and C to go back.

## Test with one command (no original artwork)

With CMake, Python 3.9+ and a C++20 compiler installed, run:

```sh
python tools/test.py
```

On Linux, use `python3 tools/test.py` if `python` is unavailable. For MinGW use
`python tools/test.py --generator "MinGW Makefiles"`. These commands configure,
compile and run the suite, returning a failing exit status if any step fails.
No SDL2, mpg123, CD, original executable, artwork, fonts, samples or music is
required. Nothing is downloaded. Tests use synthetic maps, sprites, pixels and
audio buffers, plus temporary save files.

Coverage includes integer physics, collision/flipper mechanisms, rules and
scoring primitives, binary formats, software mixing, save failure handling,
viewport/nudge presentation and source dependency boundaries. Windows adds
synthetic image-scaling checks; Linux and macOS add real filesystem save/locking
checks.
This suite does not establish complete gameplay fidelity or test a real audio
device/controller. GitHub Actions runs the same command on Windows, Linux and
macOS.

## Development and rights

See [CONTRIBUTING.md](CONTRIBUTING.md) for the source layout, coding conventions
and testing instructions.

The reconstructed source is available under the [MIT License](LICENSE).
The original game name and all original artwork, sounds, music and other game
data belong to their respective rights holders. The MIT license does not cover
those assets or grant rights to the original game's name or trademarks.
