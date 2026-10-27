# Satellite Visualization

## Description

## Building

### Windows

Build scripts and projects are generated through CMake.
To generate a Visual Studio project, navigate to the project directory and type the following in a powershell window:

```bash
# For Visual Studio 2026
cmake -G"Visual Studio 18 2026" .

# For Visual Studio 2022
cmake -G"Visual Studio 17 2022" .
```

Afterwards, open the generated `.slnx` file in Visual Studio.

If there are any difficulties with executing the program after it compiles, Visual Studio may have selected `ALL_BUILD` instead of the actual project. In that case, just right-click the project `space`, and select `Set As Startup Project`.

### macOS

To build for macOS, you can either generate a Makefile, Ninja scripts or an XCode project.

```bash
# ninja
cmake -G"Ninja"
ninja
```

### Web

The web build uses [Emscripten](https://emscripten.org/). All assets assets are packed into `space.data`.

```bash
emcmake cmake -S . -B build-web -G"Ninja"
ninja -C build-web
```

The page has to be served over HTTP (opening the `.html` file directly won't load the data):

```bash
python3 -m http.server 8080 --directory build-web
```

Then open `http://localhost:8080/space.html`.
