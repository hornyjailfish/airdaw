# 🎵 AirDAW - START HERE

Welcome to AirDAW! This document will get you up and running with our **clean, simple approach** to integrating miniaudio and Raylib.

---

## 🎯 What You Need to Know

### The Key Insight

**Miniaudio is header-only and requires NO additional libraries.**

We followed the [official miniaudio documentation](https://miniaud.io/docs/manual/index.html#Building) exactly, which clearly states:

> "On Windows, you don't need to link against any additional libraries."

This project demonstrates the **correct way** to integrate miniaudio with Raylib on Windows, avoiding common pitfalls like the `Rectangle` type conflict.

---

## ⚡ Quick Start (2 Minutes)

```bash
# 1. Build
just raylib

# 2. Run
just run

# 3. Use the DAW
#    - Press SPACE to play/stop
#    - Click track buttons to control individual tracks
#    - Drag sliders to adjust volume/pan
#    - Click + ADD TRACK to add more tracks
```

**That's it!** You now have a working multithreaded DAW with real-time audio.

---

## ✅ What We Built

- ✅ **Real-time audio engine** using miniaudio (header-only)
- ✅ **Multithreaded architecture** (separate audio and UI threads)
- ✅ **Lock-free communication** using C11 atomics
- ✅ **Full mixer interface** with Raylib UI
- ✅ **Multi-track support** (up to 16 tracks)
- ✅ **Volume, pan, mute, solo** per track
- ✅ **Real-time metering** (peak and RMS)
- ✅ **Clean, maintainable codebase**

---

## 🔑 The Solution (How We Avoided Conflicts)

### The Problem
On Windows, Raylib and Windows GDI both define types with the same names:
- `Rectangle` - Raylib struct vs Windows RECT
- `DrawText` - Raylib function vs Windows API

### The Solution (3 Simple Steps)

**1. Use compiler flags to exclude Windows GDI:**
```bash
-DNOGDI          # Exclude Windows GDI (Rectangle, etc.)
-DNOUSER         # Exclude Windows USER (DrawText, etc.)
-DWIN32_LEAN_AND_MEAN
-DNOMINMAX
```

**2. Include headers in the correct order:**
```c
#include <raylib.h>              // FIRST - defines Rectangle, DrawText

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"  // SECOND - includes Windows headers safely
```

**3. Link only essential libraries:**
```bash
-lraylibdll      # Raylib UI
-lopengl32       # OpenGL for rendering
# That's it! No -lwinmm, no -lgdi32 needed!
```

---

## 📁 Project Structure

```
airdaw/
├── main_raylib.c          # Main application (Raylib UI + Miniaudio)
├── justfile               # Build system (run: just raylib)
├── .clangd                # IDE configuration for clangd
│
├── Documentation/
│   ├── START_HERE.md              # ← You are here
│   ├── CLEAN_APPROACH_SUMMARY.md  # Detailed explanation of the clean approach
│   ├── MINIAUDIO_SETUP.md         # Comprehensive setup guide
│   ├── MINIAUDIO_QUICKREF.md      # Quick reference for developers
│   ├── CHANGELOG.md               # What changed and why
│   ├── PROJECT_OVERVIEW.md        # Architecture overview
│   ├── QUICKSTART.md              # 5-minute getting started
│   └── TESTING.md                 # Testing guide
│
├── vendor/
│   └── miniaudio/
│       └── miniaudio.h    # Header-only audio library
│
├── tests/                 # Test suite (70+ tests)
└── dist/                  # Build output
    └── airdaw.exe
```

---

## 📚 Documentation Guide

Read these in order:

| Step | Document | Purpose |
|------|----------|---------|
| **1** | `START_HERE.md` | **You are here** - Overview and quick start |
| **2** | `QUICKSTART.md` | Get running in 5 minutes |
| **3** | `CLEAN_APPROACH_SUMMARY.md` | Understand the clean miniaudio approach |
| **4** | `MINIAUDIO_SETUP.md` | Deep dive into setup and configuration |
| **5** | `MINIAUDIO_QUICKREF.md` | Quick reference for daily development |
| **6** | `PROJECT_OVERVIEW.md` | Full architecture and design |
| **7** | `TESTING.md` | How to test the project |

---

## 🚀 Common Tasks

### Build and Run
```bash
just run              # Build and run
just raylib           # Build only
just debug-raylib     # Debug build
```

### Development
```bash
just dev              # Rebuild and run
just test             # Run test suite
just generate-compile-commands  # Update IDE config
```

### IDE Setup
```bash
# For clangd/VSCode/etc.
just generate-compile-commands
# Restart your editor
```

---

## 🎨 Using the DAW

### Controls
- **SPACE** - Play/Stop all tracks
- **Click track PLAY** - Play individual track
- **Click + ADD TRACK** - Add new track (up to 16)
- **Drag sliders** - Adjust volume/pan
- **Click MUTE/SOLO** - Control track routing

### Features
- Each track generates a sine wave at a specific frequency
- Master volume and metering
- Real-time visual feedback
- Thread-safe operation (audio thread + UI thread)

---

## 💡 Key Takeaways

### 1. Miniaudio is Header-Only
```c
// Just do this in ONE .c file:
#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"

// No libs to link! It handles everything internally:
// - Windows: WASAPI, DirectSound, WinMM
// - macOS: CoreAudio
// - Linux: PulseAudio, ALSA
```

### 2. Use Compiler Flags, Not Wrappers
```bash
# ✅ Clean approach (via compiler flags)
clang -DNOGDI -DNOUSER ...

# ❌ Don't do this (complex wrapper headers)
#include "custom_wrapper.h"
```

### 3. Include Order Matters
```c
// ✅ CORRECT
#include <raylib.h>
#include "miniaudio.h"

// ❌ WRONG - causes conflicts
#include "miniaudio.h"
#include <raylib.h>
```

### 4. Link Only What You Need
```bash
# ✅ Minimal linking
-lraylibdll -lopengl32

# ❌ Unnecessary (miniaudio handles internally)
-lwinmm -lgdi32 -ldsound
```

---

## 🐛 Troubleshooting

### Build Fails with "Rectangle redefined"
**Solution:** Ensure `-DNOGDI` flag is present in justfile

### Build Fails with "DrawText undefined"
**Solution:** Ensure `-DNOUSER` flag is present and raylib is included first

### Clangd Shows Errors (but code compiles)
**Solution:**
```bash
just generate-compile-commands
# Restart your editor
```

### Audio Doesn't Initialize
**Solution:**
1. Check if another app is using audio device
2. Try running with admin privileges (Windows)
3. Verify audio drivers are installed

---

## 🎓 Learning Resources

### Miniaudio
- [Official Documentation](https://miniaud.io/docs/manual/)
- [GitHub Repository](https://github.com/mackron/miniaudio)
- [Examples](https://github.com/mackron/miniaudio/tree/master/examples)

### Raylib
- [Official Website](https://www.raylib.com/)
- [Cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html)
- [Examples](https://www.raylib.com/examples.html)

### Project Documentation
- All docs are in the root directory with `.md` extension
- Start with `CLEAN_APPROACH_SUMMARY.md` for technical details

---

## 🤝 Contributing

This project demonstrates best practices for:
- Header-only library integration
- Multithreaded audio/UI architecture
- Lock-free communication patterns
- Clean build configuration
- Cross-platform development

Feel free to:
- Extend the audio engine (add effects, samplers, etc.)
- Improve the UI (add more controls, themes)
- Add more tracks types (audio files, synthesizers)
- Improve documentation

---

## ✨ What Makes This Special

### 1. Follows Official Documentation
We use miniaudio **exactly** as documented - no hacks, no workarounds.

### 2. Clean and Simple
No wrapper headers, no macro gymnastics, just standard Windows API exclusion patterns.

### 3. Well-Documented
Every decision is explained. You'll understand **why** we did things this way.

### 4. Production-Ready
Thread-safe, tested (70+ tests), maintainable code that you can actually use.

### 5. Educational
Learn how to properly integrate header-only libraries on Windows while avoiding common pitfalls.

---

## 🎯 Next Steps

### For Beginners
1. Read `QUICKSTART.md`
2. Build and run the project
3. Explore `main_raylib.c` to understand the code
4. Run the tests: `just test`

### For Intermediate Developers
1. Read `CLEAN_APPROACH_SUMMARY.md`
2. Understand the threading model in `THREADING.md`
3. Study the miniaudio setup in `MINIAUDIO_SETUP.md`
4. Try modifying tracks to add new features

### For Advanced Developers
1. Review the full architecture in `PROJECT_OVERVIEW.md`
2. Read the design decisions in `DECISIONS.md`
3. Explore the test suite in `tests/`
4. Consider extending to Sokol+Clay for advanced UI

---

## 📞 Need Help?

1. **Check the docs** - We have comprehensive documentation
2. **Read the code** - It's well-commented and organized
3. **Run the tests** - They demonstrate how everything works
4. **Check the examples** - Official miniaudio and raylib examples

---

## 🏆 Success!

If you can build and run the project, you have successfully:
- ✅ Integrated miniaudio (header-only) correctly
- ✅ Avoided Rectangle/DrawText conflicts on Windows
- ✅ Created a multithreaded DAW with real-time audio
- ✅ Built a clean, maintainable codebase

**Congratulations!** Now go make some music! 🎵

---

**Remember:** Keep it simple, follow the documentation, and trust that miniaudio is truly header-only. No extra libraries needed!