# Miniaudio + Raylib Quick Reference

## 🚀 Quick Start

```bash
# Build and run
just run

# Build only
just raylib

# Debug build
just debug-raylib

# Run tests
just test
```

## 📝 Essential Code Pattern

```c
// Include order matters!
#include <raylib.h>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"

// Your code here...
```

## 🔧 Required Compiler Flags

```bash
-DNOGDI                  # Exclude Windows GDI (Rectangle conflict)
-DNOUSER                 # Exclude Windows USER (DrawText conflict)
-DWIN32_LEAN_AND_MEAN    # Exclude rarely-used Windows APIs
-DNOMINMAX               # Prevent min/max macro conflicts
```

## 📦 Libraries to Link

```bash
# ✅ Required
-lraylibdll              # Raylib (or -lraylib for static)
-lopengl32               # OpenGL for Raylib

# ❌ NOT needed (miniaudio handles internally)
# -lwinmm                # Miniaudio is header-only!
# -lgdi32                # NOGDI flag prevents GDI usage
# -ldsound               # Not needed
```

## 🎵 Miniaudio Basics

### Initialize Device

```c
ma_device_config config = ma_device_config_init(ma_device_type_playback);
config.playback.format = ma_format_f32;
config.playback.channels = 2;
config.sampleRate = 48000;
config.dataCallback = audio_callback;
config.pUserData = &my_engine;

ma_device device;
if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
    // Handle error
}

ma_device_start(&device);
```

### Audio Callback

```c
void audio_callback(ma_device* device, void* output, 
                    const void* input, ma_uint32 frame_count) {
    float* out = (float*)output;
    
    for (ma_uint32 i = 0; i < frame_count; i++) {
        out[i * 2 + 0] = /* left channel */;
        out[i * 2 + 1] = /* right channel */;
    }
}
```

### Cleanup

```c
ma_device_uninit(&device);
```

## 🎨 Raylib UI Patterns

### Button

```c
bool draw_button(Rectangle bounds, const char* text, bool active) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    bool clicked = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
    
    Color color = active ? ACTIVE_COLOR : 
                  hovered ? HOVER_COLOR : NORMAL_COLOR;
    DrawRectangleRec(bounds, color);
    DrawText(text, bounds.x + 10, bounds.y + 10, 10, WHITE);
    
    return clicked;
}
```

### Vertical Slider

```c
float draw_vertical_slider(Rectangle bounds, float value) {
    bool dragging = CheckCollisionPointRec(GetMousePosition(), bounds) &&
                    IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    
    if (dragging) {
        Vector2 mouse = GetMousePosition();
        value = 1.0f - ((mouse.y - bounds.y) / bounds.height);
        value = fmaxf(0.0f, fminf(1.0f, value));
    }
    
    DrawRectangleRec(bounds, BG_COLOR);
    float fill_height = bounds.height * value;
    DrawRectangle(bounds.x, bounds.y + bounds.height - fill_height, 
                  bounds.width, fill_height, FILL_COLOR);
    
    return value;
}
```

### Meter

```c
void draw_meter(Rectangle bounds, float level) {
    Color color = level > 0.9f ? RED : 
                  level > 0.7f ? YELLOW : GREEN;
    
    float fill_height = bounds.height * level;
    DrawRectangle(bounds.x, bounds.y + bounds.height - fill_height,
                  bounds.width, fill_height, color);
}
```

## 🐛 Common Issues & Fixes

### Rectangle type conflict
```bash
# Symptom: "error: conflicting types for 'Rectangle'"
# Fix: Ensure -DNOGDI flag is present
just generate-compile-commands
# Restart IDE
```

### DrawText undefined
```bash
# Symptom: "undefined reference to 'DrawTextA'"
# Fix: Ensure -DNOUSER flag and raylib is included first
# Check include order in main_raylib.c
```

### Clangd shows errors but code compiles
```bash
# Fix: Regenerate compile commands
just generate-compile-commands
# Restart your editor
```

### Audio doesn't start
```c
// Check return values
if (ma_device_init(NULL, &config, &device) != MA_SUCCESS) {
    printf("Device init failed!\n");
}
if (ma_device_start(&device) != MA_SUCCESS) {
    printf("Device start failed!\n");
}
```

## 📊 Performance Tips

### Development Build (Fast Compile)
```bash
clang -std=c11 -O0 -g ... main_raylib.c ...
```

### Release Build (Optimized)
```bash
clang -std=c11 -O3 ... main_raylib.c ...
```

### Reduce Recompilation
- Keep miniaudio in separate file if frequently editing
- Use forward declarations where possible
- Split large functions into smaller ones

## 🧪 Testing

```bash
# All tests
just test

# Unit tests only (fast)
just test-unit

# Integration tests (requires audio device)
just test-integration

# Build tests without running
just test-build
```

## 📁 Project Structure

```
airdaw/
├── main_raylib.c          # Main application (Raylib UI)
├── main.c                 # Sokol version (advanced)
├── justfile               # Build system
├── .clangd                # IDE configuration
├── vendor/
│   ├── miniaudio/
│   │   └── miniaudio.h    # Header-only audio library
│   ├── clay/              # UI layout (Sokol version)
│   └── ctest/             # Testing framework
└── tests/                 # Test suite
```

## 🔍 Debugging

### Print Audio Info
```c
printf("Backend: %s\n", ma_get_backend_name(device.pContext->backend));
printf("Format: %s\n", ma_get_format_name(device.playback.format));
printf("Channels: %u\n", device.playback.channels);
printf("Sample Rate: %u\n", device.sampleRate);
```

### Check Audio Callback
```c
void audio_callback(...) {
    static int frame_counter = 0;
    if (++frame_counter % 480 == 0) {  // Every 10ms @ 48kHz
        printf("Audio callback running\n");
    }
    // ... rest of callback
}
```

### Verify No Clipping
```c
for (ma_uint32 i = 0; i < frame_count * 2; i++) {
    if (out[i] > 1.0f || out[i] < -1.0f) {
        printf("CLIPPING at sample %u: %f\n", i, out[i]);
    }
}
```

## 📚 Documentation

- `MINIAUDIO_SETUP.md` - Detailed setup guide
- `PROJECT_OVERVIEW.md` - Architecture overview
- `TESTING.md` - Testing guide
- `CHANGELOG.md` - Recent changes
- [Miniaudio Docs](https://miniaud.io/docs/manual/)
- [Raylib Cheatsheet](https://www.raylib.com/cheatsheet/cheatsheet.html)

## 💡 Best Practices

1. **Always check return values** from miniaudio functions
2. **Keep audio callback real-time safe** (no malloc, no locks, no I/O)
3. **Use atomic operations** for shared state between threads
4. **Include raylib before miniaudio** (order matters!)
5. **Use compiler flags** instead of wrapper headers
6. **Test on real hardware** (not just VMs)
7. **Profile before optimizing** (measure, don't guess)

## ⚡ Keyboard Shortcuts (In App)

- `SPACE` - Play/Stop All
- `ESC` - Quit

## 🆘 Need Help?

1. Check `MINIAUDIO_SETUP.md` for detailed explanations
2. Run `just check` to verify dependencies
3. Run `just test` to verify functionality
4. Check GitHub issues for similar problems
5. Read [miniaudio documentation](https://miniaud.io/docs/manual/)

---

**Remember:** Miniaudio is header-only. No extra libraries needed! 🎵