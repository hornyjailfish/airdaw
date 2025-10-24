# Miniaudio Setup Guide

## Overview

This document explains AirDAW's clean, simple approach to integrating **miniaudio** following the [official documentation](https://miniaud.io/docs/manual/index.html#Building).

## Key Principles

### 1. Miniaudio is Header-Only

Miniaudio is a **single-file, header-only** audio library. This means:

- ✅ No separate compilation required
- ✅ No additional libraries to link (on Windows or any platform)
- ✅ Platform backends (WASAPI, DirectSound, CoreAudio, etc.) are handled automatically
- ✅ Just include the header and define `MINIAUDIO_IMPLEMENTATION` in **one** .c file

### 2. No Extra Windows Libraries Needed

According to the [miniaudio documentation](https://miniaud.io/docs/manual/index.html#Building):

> "On Windows, you don't need to link against any additional libraries."

This is because miniaudio uses the Windows API directly and doesn't require linking against `winmm.lib`, `dsound.lib`, or other audio-specific libraries.

### 3. Clean Implementation Without Wrappers

Our previous approach used complex wrapper headers to avoid conflicts. The **new, clean approach** uses compiler flags instead.

## The Rectangle/DrawText Conflict

### Problem

On Windows, both Raylib and Windows GDI define types/functions with the same names:
- `Rectangle` (Raylib struct vs Windows RECT typedef)
- `DrawText` (Raylib function vs Windows DrawTextA/DrawTextW)
- `CloseWindow` (Raylib function vs Windows API)
- `ShowCursor` (Raylib function vs Windows API)

When miniaudio includes Windows headers, these conflicts cause compilation errors or incorrect behavior.

### Solution: Compiler Flags

Instead of wrapper headers, we use preprocessor macros **via compiler flags** to exclude Windows GDI:

```bash
-DNOGDI          # Exclude GDI types (Rectangle, etc.)
-DNOUSER         # Exclude USER API (DrawText, etc.)
-DWIN32_LEAN_AND_MEAN  # Exclude rarely-used Windows APIs
-DNOMINMAX       # Prevent min/max macro conflicts
```

These flags prevent Windows headers from defining conflicting types, allowing Raylib's definitions to take precedence.

## Implementation

### File Structure

```
main_raylib.c
├── Compiler flags (defined in justfile)
│   ├── -DNOGDI
│   ├── -DNOUSER
│   ├── -DWIN32_LEAN_AND_MEAN
│   └── -DNOMINMAX
│
├── #include <raylib.h>           (included first)
│
├── #define MINIAUDIO_IMPLEMENTATION
├── #include "vendor/miniaudio/miniaudio.h"
│
└── Rest of code...
```

### Order of Includes

**Critical:** Include Raylib **before** miniaudio:

```c
// ✅ CORRECT ORDER
#include <raylib.h>
#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"

// ❌ WRONG ORDER - will cause conflicts
#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"
#include <raylib.h>
```

### Justfile Configuration

```makefile
# Compiler flags
RAYLIB_DEFINES := "-D_CRT_SECURE_NO_WARNINGS -DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX"

# Include paths
RAYLIB_INCLUDES := "-IC:/path/to/raylib/include -Ivendor -Ivendor/miniaudio"

# Libraries (NO miniaudio libs needed!)
RAYLIB_LIBS_WIN := "-LC:/path/to/raylib/lib -lraylibdll -lopengl32"

# Build command
raylib:
    clang {{RAYLIB_CFLAGS}} {{RAYLIB_DEFINES}} {{RAYLIB_INCLUDES}} main_raylib.c {{RAYLIB_LIBS_WIN}} -o dist/airdaw.exe
```

**Note:** Only Raylib and OpenGL libraries are linked. No `-lwinmm`, `-lgdi32`, `-ldsound`, etc.

## Clangd Configuration

For proper IDE support, configure `.clangd`:

```yaml
CompileFlags:
  Add:
    - "-std=c11"
    - "-Wall"
    - "-Wextra"
    
    # Platform-specific defines (prevent Windows GDI conflicts)
    - "-D_CRT_SECURE_NO_WARNINGS"
    - "-DNOGDI"
    - "-DNOUSER"
    - "-DWIN32_LEAN_AND_MEAN"
    - "-DNOMINMAX"
    
    # Include paths (Raylib first!)
    - "-IC:/path/to/raylib/include"
    - "-Ivendor"
    - "-Ivendor/miniaudio"
```

The same flags used for compilation must be present in `.clangd` for proper type resolution.

## What We Removed

### Old Approach (Overcomplicated)

```c
// ❌ OLD: Used wrapper header
#include "raylib_wrapper.h"  // Complex macro gymnastics

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"

// ❌ OLD: Manually undefine Windows function renames
#ifdef _WIN32
    #undef CloseWindow
    #undef ShowCursor
    #undef DrawText
    // ... many more undefs
#endif
```

**Problems with old approach:**
- Required maintaining a separate wrapper header
- Fragile - easy to break with updates
- Confusing - mixed macro redefinitions
- Hard to debug

### New Approach (Clean)

```c
// ✅ NEW: No wrapper needed!
#include <raylib.h>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"

// That's it! Compiler flags handle everything
```

**Benefits:**
- ✅ Simple and maintainable
- ✅ Follows miniaudio documentation exactly
- ✅ No wrapper headers to maintain
- ✅ Works with clangd out of the box
- ✅ Easy to understand

## Miniaudio Usage

### Basic Setup

```c
// 1. Define the implementation in ONE .c file
#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"

// 2. Create device configuration
ma_device_config config = ma_device_config_init(ma_device_type_playback);
config.playback.format = ma_format_f32;
config.playback.channels = 2;
config.sampleRate = 48000;
config.dataCallback = audio_callback;
config.pUserData = &my_engine;

// 3. Initialize device (miniaudio handles platform backend automatically)
ma_device device;
if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
    printf("Failed to initialize audio device\n");
    return false;
}

// 4. Start device
if (ma_device_start(&device) != MA_SUCCESS) {
    printf("Failed to start audio device\n");
    ma_device_uninit(&device);
    return false;
}

// 5. Audio callback (runs on audio thread)
void audio_callback(ma_device* device, void* output, const void* input, ma_uint32 frame_count) {
    float* out = (float*)output;
    // Generate/process audio here
}

// 6. Cleanup
ma_device_uninit(&device);
```

### Platform Backend Selection

Miniaudio automatically selects the best backend:

- **Windows:** WASAPI (preferred), DirectSound (fallback), WinMM (fallback)
- **macOS:** CoreAudio
- **Linux:** PulseAudio, ALSA, JACK
- **iOS:** CoreAudio
- **Android:** AAudio, OpenSL ES

**No code changes needed** - miniaudio handles this automatically!

## Testing

### Build and Run

```bash
# Build
just raylib

# Run
just run

# Expected output:
# "Audio engine initialized: 48000 Hz, 2 channels"
# "Added track 0: Bass (110.0 Hz)"
# ...
```

### Verify No Extra Libs

Check the build command output:

```bash
clang ... main_raylib.c -lraylibdll -lopengl32 -o dist/airdaw.exe
```

You should see:
- ✅ `-lraylibdll` (Raylib)
- ✅ `-lopengl32` (OpenGL for Raylib)
- ❌ NO `-lwinmm`
- ❌ NO `-lgdi32`
- ❌ NO `-ldsound`

## Troubleshooting

### Issue: Rectangle type conflicts

**Symptom:** Compiler errors about `Rectangle` being redefined

**Solution:** Ensure `-DNOGDI` flag is present in both justfile and `.clangd`

### Issue: DrawText undefined

**Symptom:** Linker errors about `DrawText` or `DrawTextA`

**Solution:** Ensure `-DNOUSER` flag is present and Raylib is included before miniaudio

### Issue: Clangd shows type errors

**Symptom:** IDE shows red squiggles even though code compiles

**Solution:** Regenerate `compile_commands.json`:

```bash
just generate-compile-commands
```

Then restart your editor/IDE.

### Issue: Audio doesn't initialize

**Symptom:** `ma_device_init()` returns error

**Solutions:**
1. Check if another application is using the audio device
2. Try running with administrator privileges (Windows only)
3. Check if audio drivers are installed and working

## Performance Notes

### Header-Only Implications

Since miniaudio is header-only with `MINIAUDIO_IMPLEMENTATION`:
- First compilation is slower (full implementation compiled)
- Incremental builds are fast (only if main_raylib.c changes)
- Binary size increases slightly (entire miniaudio code included)

### Optimization

For faster compilation during development:

```makefile
# Development build (fast compilation)
dev:
    clang -std=c11 -O0 -g {{RAYLIB_DEFINES}} {{RAYLIB_INCLUDES}} main_raylib.c {{RAYLIB_LIBS_WIN}} -o dist/airdaw_dev.exe

# Release build (optimized)
release:
    clang -std=c11 -O3 {{RAYLIB_DEFINES}} {{RAYLIB_INCLUDES}} main_raylib.c {{RAYLIB_LIBS_WIN}} -o dist/airdaw.exe
```

## References

- [Miniaudio Documentation](https://miniaud.io/docs/manual/index.html)
- [Miniaudio GitHub](https://github.com/mackron/miniaudio)
- [Raylib Documentation](https://www.raylib.com/)
- [Windows API Macros](https://learn.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers)

## Summary

The **clean miniaudio approach**:

1. ✅ No wrapper headers needed
2. ✅ No extra Windows libraries to link
3. ✅ Uses compiler flags to prevent conflicts
4. ✅ Follows miniaudio documentation exactly
5. ✅ Works seamlessly with clangd
6. ✅ Simple and maintainable

**Bottom line:** Miniaudio is truly header-only. Trust the documentation! 🎵