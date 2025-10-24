# AirDAW Quick Start Guide

Get up and running with AirDAW in 5 minutes!

## What You'll Build

A simple DAW with:
- Multi-track audio engine (real-time mixing)
- Volume, pan, mute, and solo controls
- Master bus with metering
- Multithreaded architecture (separate audio and UI threads)

## Prerequisites

✅ **Windows** (primary platform for this guide)
✅ **Clang or GCC** compiler
✅ **Raylib** library (for UI - easiest option)

### Install Clang (Windows)

**Option 1: Via Scoop (Recommended)**
```powershell
# Install Scoop package manager
iwr -useb get.scoop.sh | iex

# Install Clang
scoop install llvm
```

**Option 2: Direct Download**
- Download from: https://releases.llvm.org/download.html
- Install and add to PATH

### Install Just (Optional but Recommended)

```powershell
scoop install just
```

Or download from: https://github.com/casey/just/releases

### Install Raylib

**Option 1: Via Scoop**
```powershell
scoop bucket add extras
scoop install raylib
```

**Option 2: Manual Download**
1. Go to https://github.com/raysan5/raylib/releases
2. Download `raylib-<version>_win64_msvc16.zip`
3. Extract to `C:\raylib`
4. Add `C:\raylib\lib` to your library path

## Setup (5 Steps)

### 1. Clone or Navigate to Project

```bash
cd C:\Users\5q\gits\airdaw
```

### 2. Get Miniaudio Header

```powershell
# Create vendor directory
New-Item -ItemType Directory -Force -Path vendor\miniaudio

# Download miniaudio
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -OutFile "vendor\miniaudio\miniaudio.h"
```

### 3. Create dist Directory

```powershell
New-Item -ItemType Directory -Force -Path dist
```

### 4. Verify Dependencies

```bash
just check
```

Expected output:
```
[OK] Miniaudio found
[OK] Raylib library found
```

### 5. Build and Run!

```bash
just run
```

That's it! 🎉

## First Run

When the app starts, you'll see:

- **Toolbar** at the top with PLAY button and "Add Track" button
- **3 tracks** by default (each playing a different frequency tone)
- **Master section** on the right with volume control and meters
- **Track controls**: Volume sliders, pan controls, mute/solo buttons

### Controls

- **SPACE** - Play/Stop transport
- **MUTE button** - Mute individual track
- **SOLO button** - Solo individual track (only soloed tracks play)
- **Volume slider** - Adjust track volume (vertical)
- **Pan slider** - Adjust stereo panning (horizontal)
- **Add Track button** - Add a new track (up to 16 total)

## Verify It's Working

1. Press **SPACE** to start playback
2. You should hear harmonious sine wave tones
3. Watch the **meters** animate (showing audio levels)
4. Click **MUTE** on Track 1 - sound should stop for that track
5. Drag **volume slider** - volume should change smoothly
6. Click **SOLO** on Track 2 - only Track 2 should play

✅ If you see and hear this, congratulations! Your DAW is working!

## Project Structure

```
airdaw/
├── main_raylib.c       ← Main code (Raylib version)
├── main.c              ← Alternative (Sokol version, needs work)
├── justfile            ← Build configuration
├── README.md           ← Full documentation
├── QUICKSTART.md       ← This file
├── SETUP.md            ← Detailed setup guide
├── THREADING.md        ← Threading architecture docs
├── UI_ENGINES.md       ← Comparison of UI options
├── dist/               ← Build output
│   └── airdaw.exe
└── vendor/
    └── miniaudio/
        └── miniaudio.h
```

## Understanding the Code

The code is organized into clear sections:

### 1. Audio Engine (Lines ~30-300)
```c
// Structures
Track - Individual audio track
AudioEngine - Main audio engine

// Functions
audio_engine_init() - Initialize audio
audio_callback() - Real-time audio processing (runs on audio thread)
audio_engine_add_track() - Add new track
```

### 2. UI Rendering (Lines ~300-600)
```c
// Helper functions
draw_button() - Draw interactive button
draw_vertical_slider() - Draw volume slider
draw_meter() - Draw peak meter

// Main UI
draw_track() - Render one track's controls
draw_master_section() - Render master bus
draw_toolbar() - Top toolbar with play button
```

### 3. Main Loop (Lines ~600-end)
```c
main() - Entry point, event loop
```

## Next Steps

### Beginner Path

1. **Modify test tones**
   - Change frequencies in `audio_engine_add_track()`
   - Try different waveforms (square, saw, triangle)

2. **Tweak UI colors**
   - Modify `g_theme` structure colors
   - Experiment with layout positions

3. **Add features**
   - Add more tracks (change MAX_TRACKS)
   - Add track name editing
   - Add preset buttons (set volume patterns)

### Intermediate Path

1. **Load audio files**
   - Use `dr_wav.h` or `stb_vorbis.c` to load WAV/OGG
   - Replace test tones with real audio

2. **Add effects**
   - Simple gain effect
   - Low-pass filter
   - Reverb (use freeverb algorithm)

3. **Timeline view**
   - Draw waveform visualization
   - Add playhead indicator
   - Implement clip positioning

### Advanced Path

1. **VST3 plugin hosting**
   - Integrate VST3 SDK
   - Load and run plugins
   - Route audio through plugin chain

2. **MIDI support**
   - Add MIDI input
   - Virtual instruments
   - MIDI sequencing

3. **Project save/load**
   - Serialize project state to JSON/XML
   - Load previous sessions
   - Implement undo/redo

## Common Issues

### "Cannot find miniaudio.h"
```bash
# Re-download miniaudio
Invoke-WebRequest -Uri "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h" -OutFile "vendor\miniaudio\miniaudio.h"
```

### "Raylib not found"
```bash
# Check if installed
clang -lraylib
# If fails, install raylib (see prerequisites)
```

### "Audio device initialization failed"
- Close other audio applications
- Check audio device isn't in exclusive mode
- Restart audio service (Windows Services → "Windows Audio")

### No sound but app runs
- Check system volume isn't muted
- Verify audio device in Windows Sound settings
- Press SPACE to start playback
- Check track isn't muted

### Crackling/glitching audio
- Increase buffer size: Change `BUFFER_SIZE` from 512 to 1024
- Close background applications
- Check CPU usage isn't at 100%

## Build Options

```bash
# Default build (Raylib version)
just run

# Debug build (with symbols)
just debug-raylib

# Clean build artifacts
just clean

# Check dependencies
just check

# Show all commands
just help
```

## Performance Tips

Current performance (on modern PC):
- **CPU usage**: ~2-5% with 3 tracks
- **Latency**: ~10ms (512 samples @ 48kHz)
- **Frame rate**: 60 FPS

To improve:
1. Increase `BUFFER_SIZE` for lower CPU (higher latency)
2. Decrease `BUFFER_SIZE` for lower latency (higher CPU)
3. Reduce track count for potato PCs
4. Use release build (`-O2` or `-O3`) for production

## Learning Resources

### Audio Programming
- [miniaudio documentation](https://miniaud.io/docs/manual/index.html)
- [Real-Time Audio Programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing)
- [Digital Signal Processing Guide](https://www.dspguide.com/)

### UI/Graphics
- [Raylib examples](https://www.raylib.com/examples.html)
- [Raylib cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html)

### DAW Architecture
- Read `THREADING.md` in this project
- [JUCE tutorials](https://docs.juce.com/master/tutorial_simple_synth_noise.html)
- [Reaper SDK](https://www.reaper.fm/sdk/)

## Get Help

- **Read the docs**: Check README.md, THREADING.md, UI_ENGINES.md
- **Check the code**: Main code is in `main_raylib.c` with comments
- **Debug**: Use `just debug-raylib` for debug build
- **Profile**: Add timing code to measure performance

## You're Ready!

You now have a working DAW engine. The foundation is solid:
- ✅ Real-time audio processing
- ✅ Thread-safe communication
- ✅ Interactive UI
- ✅ Modular architecture

Build something awesome! 🎵🎸🎹

---

**Pro tip**: Start small. Get one feature working perfectly before adding the next. A simple, working DAW is better than a complex, broken one.

Happy coding! 🚀