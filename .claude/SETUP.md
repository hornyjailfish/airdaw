# AirDAW Setup Guide

This guide will help you set up all the dependencies needed to build AirDAW.

## Prerequisites

- **C11 Compiler**: Clang (recommended) or GCC
  - Windows: Install [LLVM/Clang](https://releases.llvm.org/download.html) or use MinGW
  - macOS: Xcode Command Line Tools (`xcode-select --install`)
  - Linux: `sudo apt install clang` or `sudo dnf install clang`

- **Just** (optional but recommended): [Installation guide](https://github.com/casey/just#installation)
  - Windows: `cargo install just` or download binary
  - macOS: `brew install just`
  - Linux: `cargo install just` or use package manager

## Installing Dependencies

All dependencies are header-only libraries that go in the `vendor/` directory.

### 1. Miniaudio

**Download**: [miniaudio GitHub](https://github.com/mackron/miniaudio)

```bash
# Create vendor directory structure
mkdir -p vendor/miniaudio

# Download miniaudio.h
curl -L https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h -o vendor/miniaudio/miniaudio.h
```

**Or manually**:
1. Go to https://github.com/mackron/miniaudio
2. Download `miniaudio.h`
3. Place it in `vendor/miniaudio/miniaudio.h`

### 2. Clay

**Download**: [Clay GitHub](https://github.com/nicbarker/clay)

```bash
# Create directory
mkdir -p vendor/clay

# Download clay.h
curl -L https://raw.githubusercontent.com/nicbarker/clay/main/clay.h -o vendor/clay/clay.h
```

**Or manually**:
1. Go to https://github.com/nicbarker/clay
2. Download `clay.h` from the main branch
3. Place it in `vendor/clay/clay.h`

### 3. Sokol

**Download**: [Sokol GitHub](https://github.com/floooh/sokol)

```bash
# Create directory
mkdir -p vendor/sokol

# Download required sokol headers
curl -L https://raw.githubusercontent.com/floooh/sokol/master/sokol_app.h -o vendor/sokol/sokol_app.h
curl -L https://raw.githubusercontent.com/floooh/sokol/master/sokol_gfx.h -o vendor/sokol/sokol_gfx.h
curl -L https://raw.githubusercontent.com/floooh/sokol/master/sokol_glue.h -o vendor/sokol/sokol_glue.h
```

**Or manually**:
1. Go to https://github.com/floooh/sokol
2. Download these three files:
   - `sokol_app.h`
   - `sokol_gfx.h`
   - `sokol_glue.h`
3. Place them in `vendor/sokol/`

### 4. Create dist directory

```bash
mkdir -p dist
```

## Automated Setup Script

### PowerShell (Windows)

Save this as `setup.ps1`:

```powershell
# Create directories
New-Item -ItemType Directory -Force -Path vendor\miniaudio
New-Item -ItemType Directory -Force -Path vendor\clay
New-Item -ItemType Directory -Force -Path vendor\sokol
New-Item -ItemType Directory -Force -Path dist

# Download miniaudio
Write-Host "Downloading miniaudio..."
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -OutFile "vendor\miniaudio\miniaudio.h"

# Download Clay
Write-Host "Downloading Clay..."
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/nicbarker/clay/main/clay.h" -OutFile "vendor\clay\clay.h"

# Download Sokol
Write-Host "Downloading Sokol headers..."
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/floooh/sokol/master/sokol_app.h" -OutFile "vendor\sokol\sokol_app.h"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/floooh/sokol/master/sokol_gfx.h" -OutFile "vendor\sokol\sokol_gfx.h"
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/floooh/sokol/master/sokol_glue.h" -OutFile "vendor\sokol\sokol_glue.h"

Write-Host "Setup complete! Run 'just check' to verify."
```

Run with: `powershell -ExecutionPolicy Bypass -File setup.ps1`

### Bash (macOS/Linux)

Save this as `setup.sh`:

```bash
#!/bin/bash

# Create directories
mkdir -p vendor/miniaudio
mkdir -p vendor/clay
mkdir -p vendor/sokol
mkdir -p dist

# Download miniaudio
echo "Downloading miniaudio..."
curl -L https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h -o vendor/miniaudio/miniaudio.h

# Download Clay
echo "Downloading Clay..."
curl -L https://raw.githubusercontent.com/nicbarker/clay/main/clay.h -o vendor/clay/clay.h

# Download Sokol
echo "Downloading Sokol headers..."
curl -L https://raw.githubusercontent.com/floooh/sokol/master/sokol_app.h -o vendor/sokol/sokol_app.h
curl -L https://raw.githubusercontent.com/floooh/sokol/master/sokol_gfx.h -o vendor/sokol/sokol_gfx.h
curl -L https://raw.githubusercontent.com/floooh/sokol/master/sokol_glue.h -o vendor/sokol/sokol_glue.h

echo "Setup complete! Run 'just check' to verify."
```

Run with: `chmod +x setup.sh && ./setup.sh`

## Verify Installation

Run the check command:

```bash
just check
```

Expected output:
```
Checking dependencies...
[OK] Clay found
[OK] Miniaudio found
[OK] Sokol found
```

Your directory structure should look like:

```
airdaw/
├── main.c
├── justfile
├── README.md
├── SETUP.md
├── dist/
└── vendor/
    ├── clay/
    │   └── clay.h
    ├── miniaudio/
    │   └── miniaudio.h
    └── sokol/
        ├── sokol_app.h
        ├── sokol_gfx.h
        └── sokol_glue.h
```

## Build and Run

Once dependencies are installed:

```bash
# Build
just build

# Or build and run
just run
```

## Troubleshooting

### "Cannot find clay.h"
- Make sure `vendor/clay/clay.h` exists
- Check file permissions

### "Cannot find miniaudio.h"
- Verify `vendor/miniaudio/miniaudio.h` exists
- Try re-downloading

### "Cannot find sokol headers"
- Ensure all three sokol headers are in `vendor/sokol/`
- Check spelling and paths

### Compiler not found
- Windows: Add Clang/GCC to PATH
- macOS: Install Xcode Command Line Tools
- Linux: Install clang via package manager

### Linker errors on Windows
- Make sure you're linking against: `kernel32 user32 gdi32 opengl32 ole32`
- These should be included in the justfile already

### Audio device initialization fails
- Windows: Check that no other app is using the audio device exclusively
- Linux: Install ALSA development packages (`libasound2-dev`)
- macOS: Should work out of the box with CoreAudio

### Graphics/Window issues
- Make sure OpenGL drivers are installed
- Update graphics drivers
- On Linux, you may need: `libx11-dev libxcursor-dev libxi-dev`

## Platform-Specific Notes

### Windows
- Uses WASAPI for audio (built into Windows)
- Requires OpenGL 3.3+ drivers
- Recommended: Use Clang with MinGW-w64

### macOS
- Uses CoreAudio (built-in)
- Requires macOS 10.13+
- Metal backend available (modify sokol defines)

### Linux
- Uses ALSA/PulseAudio
- Requires X11 development libraries
- Install packages: `sudo apt install libasound2-dev libx11-dev libxcursor-dev libxi-dev`

## Next Steps

After setup:

1. **Build the project**: `just build`
2. **Run it**: `just run`
3. **Press Space** to start/stop audio playback
4. **Read the code** in `main.c` to understand the structure
5. **Implement Clay rendering** (see TODO in main.c)
6. **Add your own features!**

## Alternative UI Engines

If you want to use a different UI engine instead of Clay + Sokol:

- **Raylib**: Easier to get started, has built-in UI controls
- **Nuklear**: Immediate mode GUI, popular for games/tools
- **Dear ImGui**: Most popular, requires C++ wrapper or C bindings
- **lvgl**: Embedded-friendly, good for custom rendering

To use a different UI engine, replace the Clay and Sokol code in `main.c` with your chosen library.

## Need Help?

- Check the dependency documentation:
  - [miniaudio docs](https://miniaud.io/docs/manual/index.html)
  - [Clay docs](https://github.com/nicbarker/clay)
  - [Sokol docs](https://github.com/floooh/sokol)
  
- Look at example projects using these libraries
- The code in `main.c` is heavily commented to help you understand

---

Happy coding! 🎵