# rF Config — Open-Source Reimplementation

A standalone, native Win32 reimplementation of the rFactor 1 / gMotor 2 video configuration utility. The code is written from scratch in C++17 and does **not** require the proprietary `rF Config.exe`, rFactor game libraries, .NET, or D3DX9 to compile.

## Features

- Native Windows dialog with the original configuration categories: GPU, resolution, refresh rate, shader level, anti-aliasing, language, VSync, windowed mode, Widescreen UI, and MultiView.
- Direct3D 9 GPU enumeration, display modes, refresh rates, and supported multisample types.
- `Config.ini` reading and writing with the `[COMPONENTS]` format used by rFactor 1.
- Preservation of unrelated `Config.ini` sections and values. A `.bak` backup is created before changes; writes use a temporary file and an atomic replacement.
- New configuration defaults for clean installations, including graphics configuration, support paths, commentary, install settings, system memory, and GPU memory when detectable.
- Discovery of translations from `Support\Languages\*.dic`.
- Video hardware test and an optional `--debug` simulated-error dialog.
- System and GPU information, support links, and optional `statsend.exe -sendstats` integration when that program exists in the installation.
- `--config <path>` or `config=<path>` to specify a non-default configuration file.

## Build (Windows)

Requirements: Windows SDK and Visual Studio 2022 with the Desktop Development with C++ workload, plus CMake 3.20 or later.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Output: `build\Release\rF Config.exe` (32-bit Win32 GUI executable).

The `.github/workflows/build.yml` workflow builds, tests, uploads an artifact, and publishes GitHub Releases for `v*` tags.

## Installation

1. Back up the original `rF Config.exe` and `Config.ini` from your rFactor installation.
2. Copy the newly built `rF Config.exe` to the same folder as `rFactor.exe` and `Config.ini`.
3. Launch `rF Config.exe`, change your video settings, and click **OK**.
4. The utility saves `Config.ini` and keeps a backup named `Config.ini.bak`.

On installations under `Program Files`, Windows may require elevated permissions to write `Config.ini`.

## Compatibility considerations

The original executable is a 32-bit application built in 2009. Its UI resources and configuration-key names were inspected to design this implementation, but it is not a byte-for-byte reconstruction and is not endorsed by Image Space Incorporated.

**Limitations requiring testing against the actual game:** rFactor's internal `VideoMode`, `VideoRefresh`, and `FSAA` numbering depends on the graphics adapter, driver, and the original engine's enumeration order. This implementation enumerates these options using Direct3D 9; its indices may differ on some hardware. Existing numeric values are left unchanged until the relevant control is modified. For perfect compatibility on a particular PC, compare the values produced by the original utility before replacing it.

The video test checks Direct3D initialization and DXT1 texture capability. It does not duplicate every historical hardware benchmark and diagnostic. The optional debug dialog provides simulated error messages, not the original game's full error-screen pipeline.

## Files

- `src/main.cpp`: Win32 UI, GPU and graphics-mode enumeration, save actions, tests and tools.
- `src/config.hpp` / `src/config.cpp`: reusable rFactor config parser and serializer.
- `src/resources.rc`, `src/resource.h`, `src/app.manifest`: Windows dialog resources, control IDs, and visual-style manifest.
- `tests/config_tests.cpp`: portable configuration read/write tests with separate, traceable existing/new cases.
- `.github/workflows/build.yml`: GitHub Actions build and release pipeline.

## License

MIT. The license covers only the original code in this project; rFactor and gMotor are the property of their respective rights holders.
