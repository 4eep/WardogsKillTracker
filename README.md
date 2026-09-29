> **This project was created entirely with OpenAI Codex.**

# Wardogs Kill Tracker

[Русская версия](README.ru.md) · [WARDOGS on Steam](https://store.steampowered.com/app/1867240/WARDOGS/)

A Windows application that automatically tracks kills in **WARDOGS** during NVIDIA recordings. It captures the selected monitor through DXGI and confirms a kill only when both events occur within the configured time window:

1. a new kill-feed row containing the player's name;
2. the kill-confirmation icon appearing in the center of the HUD.

For every confirmed kill, the application records a timecode and saves a PNG crop of the matching kill-feed row. When recording ends, the video, text report, and images are moved into a separate directory named after the video.

## Features

- DXGI screen capture without reading game memory;
- OpenCV-based UI recognition;
- combined kill-feed and center-icon validation;
- duplicate suppression when kill-feed rows move;
- automatic NVIDIA recording start and stop detection using `Alt+F9` and the recording directory;
- timecode and kill-feed snapshot export;
- calibration and offline frame-testing tools.

## Requirements

- Windows 10 or Windows 11 x64;
- Visual Studio 2022 or newer with **Desktop development with C++**;
- CMake 3.24 or newer;
- [vcpkg](https://github.com/microsoft/vcpkg);
- OpenCV 4 and nlohmann-json.

## Building

Install the dependencies with vcpkg:

```powershell
C:\vcpkg\vcpkg.exe install opencv4:x64-windows nlohmann-json:x64-windows
```

Open PowerShell in the repository root and run the build script:

```powershell
cd E:\WardogsKillTracker
.\build.ps1
```

The Release executable will be created at `build/Release/WardogsKillTracker.exe`. To run the tests after building:

```powershell
.\build.ps1 -RunTests
```

The script waits for Enter before closing so that errors remain visible. Use `-NoPause` for automated execution. It configures CMake with `--fresh`, preventing an old cache created without vcpkg from hiding OpenCV.

Choose another configuration or build directory with parameters:

```powershell
.\build.ps1 -Configuration Debug -BuildDirectory build-debug
```

The default vcpkg location is `C:\vcpkg`. Set `VCPKG_ROOT` when it is installed elsewhere:

```powershell
$env:VCPKG_ROOT = 'D:\tools\vcpkg'
.\build.ps1
```

Equivalent manual commands:

```powershell
cmake --fresh -S . -B build -A x64 `
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Run the application with `.\build\Release\WardogsKillTracker.exe`. CMake copies `config.json` and the `assets` directory next to the executable automatically.

## Initial setup

1. Open `config.json` next to the executable.
2. Set `nvidia.recordingDirectory` to the directory used by NVIDIA App for recordings.
3. Find the index of the monitor to capture:

   ```powershell
   .\WardogsKillTracker.exe --list-monitors
   ```

4. Store the index in `capture.monitorIndex`.
5. Create or replace `assets/player_template.png` with a crop of the player's name from the kill feed. To prepare a diagnostic frame, run:

   ```powershell
   .\WardogsKillTracker.exe --create-template
   ```

6. Verify the capture regions and recognition settings:

   ```powershell
   .\WardogsKillTracker.exe --calibrate
   ```

Region coordinates are expressed as fractions of the monitor image from `0.0` to `1.0`, so they are not tied directly to a specific resolution.

## Usage

Start `WardogsKillTracker.exe` before starting an NVIDIA recording. The first `Alt+F9` press starts a session and the second ends it. The application waits until NVIDIA releases the video file and then creates the result directory:

```text
NVIDIA/Wardogs/recording.mp4
→ NVIDIA/Wardogs/recording/
  ├─ recording.mp4
  ├─ recording.txt
  ├─ 00-13-24-28.png
  └─ 00-18-07-03.png
```

The `recording.txt` report contains entries such as:

```text
Kill 00:13:24:28
Kill 00:18:07:03
```

Technical logs are written to `logs/latest.log`. Until the video is moved successfully, temporary results remain in the `output` directory next to the application.

## Diagnostics

Test one full-screen frame:

```powershell
.\WardogsKillTracker.exe --test-image C:\frames\frame.png
```

Test a frame sequence:

```powershell
.\WardogsKillTracker.exe --test-folder C:\frames\sequence
```

When `detection.debugPreview` is enabled, the application displays OpenCV debug windows. Diagnostic images are saved in the `debug` directory.

## Key configuration options

- `capture.fps` — screen analysis frequency;
- `capture.killFeedRegion` — kill-feed region;
- `detection.templateThreshold` — player-name matching threshold;
- `detection.hashDistanceThreshold` — allowed row difference during duplicate suppression;
- `detection.centerKillConfirmation.templateThreshold` — center-icon matching threshold;
- `detection.centerKillConfirmation.fusionWindowMs` — maximum interval for combining both detection signals;
- `timestampOffsetMs` — timecode correction in milliseconds.

Thresholds that are too low increase false positives, while overly high values may miss real events. Recalibrate after changing the game resolution or UI scale.

## Repository structure

```text
assets/       OpenCV templates
src/          application source code
tests/        automated tests
CMakeLists.txt
config.json   example configuration
```

## Limitations

The application is designed for Windows, a single captured monitor, and the current WARDOGS HUD. Game updates may require new templates or adjusted region coordinates.
