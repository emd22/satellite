# Satellite Visualization

## Description



## Building

### Windows

Build scripts and projects are generated through CMake.
To generate a Visual Studio project, navigate to the project directory and type the following in a powershell window:
```
# For Visual Studio 2026
cmake -G"Visual Studio 18 2026" .

# For Visual Studio 2022
cmake -G"Visual Studio 17 2022" .
```

Afterwards, open the generated `.slnx` file in Visual Studio.

If there are any difficulties with executing the program after it compiles, Visual Studio may have selected `ALL_BUILD` instead of the actual project. In that case, just right-click the project `space`, and select `Set As Startup Project`.

### Building on Linux

Verified on Ubuntu 24.04 with GCC and Clang (the same matrix as
`.github/workflows/linux.yml`). The Linux build needs CMake 3.23 or newer
and a C++20 compiler.

Install the build dependencies:

```bash
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  build-essential clang cmake git pkg-config \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxi-dev libwayland-dev libxkbcommon-dev \
  wayland-protocols
```

Then configure and build. [`scripts/build_linux.sh`](scripts/build_linux.sh)
does exactly these two steps (it installs nothing):

```bash
./scripts/build_linux.sh
```

Or run the equivalent CMake commands yourself:

```bash
cmake -S . -B build
cmake --build build --parallel
```

To build with Clang instead of the default GCC:

```bash
CC=clang CXX=clang++ ./scripts/build_linux.sh build-clang
# or: CC=clang CXX=clang++ cmake -S . -B build-clang && cmake --build build-clang --parallel
```

Notes:

- If a system raylib 5.5 is installed, CMake uses it (`find_package(raylib
  5.5)`). Otherwise CMake fetches and builds raylib 5.5 from source with
  FetchContent, which needs network access on the first configure and puts
  the download under `build/_deps/`.
- The binary is written to `build/space` (or `build-clang/space`).

#### Running on Linux

Run the app from the repository root, not from inside `build/`. Assets are
loaded through paths relative to the working directory (`ASSET_BASE_DIR`
is `"."`), so the binary must be started where `earth.glb`, `Shaders/`,
`Datasets/`, and the other data files live:

```bash
./build/space
```

On a machine without a display, run it headless with Xvfb and software GL:

```bash
sudo apt-get install -y xvfb mesa-utils
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a -s "-screen 0 1280x720x24" ./build/space
```

#### Troubleshooting (Linux)

- **Asset load warnings:** run from the repository root and keep asset
  filenames unchanged, including lowercase `earth.glb`. The Linux CI
  smoke check fails on crashes and raylib `WARNING:` or `ERROR:` lines.
- **`raylib` not found / FetchContent download fails:** the first
  configure downloads raylib 5.5, so it needs network access. Behind a
  proxy or offline, install raylib 5.5 yourself and point CMake at it
  with `CMAKE_PREFIX_PATH`.
- **Window does not open / `Failed to open display`:** the app needs an
  X11 or Wayland display. Use the Xvfb command above on headless machines.
- **OpenGL errors under Xvfb:** keep `LIBGL_ALWAYS_SOFTWARE=1` so Mesa
  uses software rendering (llvmpipe).
