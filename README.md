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
