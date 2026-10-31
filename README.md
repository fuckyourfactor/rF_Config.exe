# rF Config — Open-source reimplementation

A Win32 C++17 implementation of the **rFactor 1 / gMotor 2** graphics configuration program.

## Original interface

The main dialog is reconstructed from the resource structure of the original 2009 `rF Config.exe`: 311 × 157 dialog units, Segoe UI 9 pt, the original control positions, title, separator, and OK/Cancel buttons. Normal operation does not show new diagnostics, links, hardware-test buttons, or custom interface groups. The original resource contains some other controls that are hidden in the normal screen; this implementation keeps these hidden.

The visible settings are video adapter, resolution, shader level, anti-aliasing, refresh rate, language, VSync, windowed mode, widescreen UI and MultiView. Unavailable features are disabled according to detected video modes and monitor configuration.

## Configuration behavior

- Reads and writes `Config.ini` alongside the executable.
- Preserves unrelated sections and settings without creating an additional `.bak` file.
- Enumerates graphics adapters and valid display modes through Direct3D 9.
- Detects Direct3D 9 nonmaskable anti-aliasing quality levels and shows `None` plus `Level 1` through the supported level (up to `Level 8`), matching the original application. Uses the original `FSAA=32` to `FSAA=39` configuration indices; multi-sampling choices are a fallback when quality levels are unavailable.
- Lists actual reported display refresh rates rather than an invented `Default` item.
- Uses game configuration keys `VideoDriver`, `VideoGUID`, `VideoMode`, `VideoRefresh`, `FSAA`, `ShaderLevel`, `LanguagesFile`, `CommentaryFile`, `VSync`, `WindowedMode`, `WidescreenUI` and `SubViews`.

## Build

Requires Windows SDK, Visual Studio 2022 C++ desktop tools and CMake 3.20+.

```powershell
cmake -S . -B build -A Win32
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The executable is generated at `build\Release\rF Config.exe`. `.github/workflows/build.yml` builds and publishes tagged releases.

## Installation

Back up the original executable and `Config.ini` yourself. Place the compiled `rF Config.exe` alongside `rFactor.exe`, then launch it and use **OK** to save the video settings. **Cancel** discards changes.

## Compatibility limits

This is independently written source code, **not** a binary-equivalent copy. The dialog layout and several original configuration flag values were verified through static examination of the supplied executable. Video-mode and refresh-rate ordering may still differ from the original on particular graphics drivers, and the original game-specific hardware detection is not reproduced completely. Changes to settings should be verified in rFactor on the target PC. The Windows executable has not been run locally; the included portable configuration tests do run on Linux.

## License

MIT for the newly written source code. rFactor and gMotor remain trademarks or property of their respective holders.
