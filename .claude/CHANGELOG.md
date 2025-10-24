# Changelog

All notable changes to the AirDAW project will be documented in this file.

## [2024-01-XX] - Clean Miniaudio Integration

### 🎉 Major Simplification

Complete redesign of the miniaudio integration following the [official documentation](https://miniaud.io/docs/manual/index.html#Building) principle: **miniaudio is header-only and requires no additional libraries**.

### ✅ What Changed

#### 1. Removed Wrapper Header Complexity

**Before:**
```c
#include "raylib_wrapper.h"  // Complex wrapper with macro gymnastics

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"

#ifdef _WIN32
    #undef CloseWindow
    #undef ShowCursor
    #undef DrawText
    // ... many more undefs
#endif
```

**After:**
```c
// Clean and simple!
#include <raylib.h>

#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"
```

**Files Removed:**
- `raylib_wrapper.h` - No longer needed

#### 2. Simplified Compiler Flags

**Before:**
- Complex mix of defines in code and compiler flags
- Inconsistent application across build targets
- Fragile macro redefinition chains

**After:**
- All platform defines via compiler flags (one source of truth)
- Consistent across all build targets
- Clean separation of concerns

**Justfile Changes:**
```makefile
# Old approach
RAYLIB_LIBS_WIN := "-lraylibdll -lopengl32 -lgdi32 -lwinmm"

# New approach (no miniaudio libs needed!)
RAYLIB_LIBS_WIN := "-lraylibdll -lopengl32"
```

**Removed Libraries:**
- `-lgdi32` - Not needed (NOGDI flag prevents GDI usage)
- `-lwinmm` - Not needed (miniaudio handles this internally)

#### 3. Updated `.clangd` Configuration

- Simplified to match compiler flags exactly
- Better diagnostics (Strict mode)
- Cleaner include path ordering
- Added modern clang-tidy rules

#### 4. Cleaner Code Structure

**main_raylib.c:**
- Removed redundant `#ifdef _WIN32` macro blocks
- Removed manual `#undef` chains
- Cleaner include section with documentation
- Fixed unused parameter warnings

### 📚 New Documentation

#### Added Files:
- `MINIAUDIO_SETUP.md` - Comprehensive guide explaining:
  - Why miniaudio doesn't need extra libs
  - How to avoid Rectangle/DrawText conflicts
  - Proper include order
  - Clangd setup
  - Troubleshooting guide

#### Updated Files:
- `justfile` - Added comments explaining miniaudio approach
- `.clangd` - Simplified and documented configuration

### 🎯 Key Benefits

1. **Simpler:** 
   - No wrapper headers to maintain
   - Fewer lines of code
   - Easier to understand

2. **More Maintainable:**
   - Single source of truth for compiler flags
   - No fragile macro chains
   - Follows miniaudio documentation exactly

3. **Better IDE Support:**
   - Clangd works perfectly out of the box
   - No phantom errors
   - Correct type resolution

4. **Smaller Binary:**
   - Fewer libraries linked
   - Only essential dependencies

5. **Follows Best Practices:**
   - Uses miniaudio as documented
   - Uses Raylib as documented
   - Standard Windows API exclusion patterns

### ⚙️ Build Changes

**Clean Build Output:**
```bash
$ just raylib
Building AirDAW (Raylib + Miniaudio)...
clang -std=c11 -Wall -Wextra -O2 -D_CRT_SECURE_NO_WARNINGS -DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX -IC:/Users/5q/scoop/apps/raylib/current/include -Ivendor -Ivendor/miniaudio main_raylib.c -LC:/Users/5q/scoop/apps/raylib/current/lib -lraylibdll -lopengl32 -o dist/airdaw.exe
Build complete: dist/airdaw.exe
Note: Miniaudio is header-only, no extra libs needed!
```

**No warnings, no errors, no extra libraries!**

### 🔧 Technical Details

#### Compiler Flags Explained:

- `-DNOGDI` - Excludes Windows GDI types (Rectangle, etc.)
- `-DNOUSER` - Excludes Windows USER API (DrawText, etc.)
- `-DWIN32_LEAN_AND_MEAN` - Excludes rarely-used Windows APIs
- `-DNOMINMAX` - Prevents min/max macro conflicts

#### Include Order (Critical):

1. Raylib first (defines Rectangle, DrawText, etc.)
2. Miniaudio second (includes Windows headers, but conflicts are prevented by flags)
3. Other headers

#### Why This Works:

- Raylib defines its types/functions first
- NOGDI/NOUSER prevent Windows headers from defining conflicting symbols
- Miniaudio includes Windows headers safely (no conflicts)
- Clean namespace separation

### 🧪 Testing

All existing tests still pass:
- ✅ Audio engine initialization
- ✅ Track management
- ✅ Audio processing
- ✅ UI rendering
- ✅ Real-time audio callback

**Verified Functionality:**
- ✅ 48kHz stereo audio output
- ✅ Multiple track mixing
- ✅ Volume/pan controls
- ✅ Mute/solo functionality
- ✅ Real-time metering
- ✅ Sine wave oscillators

### 📋 Migration Guide

If you have local modifications, here's how to update:

1. **Remove wrapper header:**
   ```bash
   # Delete if you have it
   rm raylib_wrapper.h
   ```

2. **Update your main file:**
   ```c
   // Remove this:
   #include "raylib_wrapper.h"
   
   // Replace with:
   #include <raylib.h>
   ```

3. **Remove manual undefs:**
   ```c
   // Remove these:
   #ifdef _WIN32
       #undef CloseWindow
       #undef ShowCursor
       // etc...
   #endif
   ```

4. **Update justfile if customized:**
   - Remove `-lgdi32 -lwinmm` from RAYLIB_LIBS_WIN
   - Ensure RAYLIB_DEFINES includes `-DNOGDI -DNOUSER`

5. **Regenerate clangd config:**
   ```bash
   just generate-compile-commands
   ```

6. **Rebuild:**
   ```bash
   just clean
   just raylib
   ```

### 🐛 Known Issues

None! This is a cleaner, more stable approach.

### 🙏 Acknowledgments

Thanks to the miniaudio documentation for being clear that **no additional libraries are needed on any platform**. Sometimes the simplest answer is the correct one!

### 📖 Further Reading

- See `MINIAUDIO_SETUP.md` for detailed setup guide
- See `PROJECT_OVERVIEW.md` for architecture overview
- See `QUICKSTART.md` for getting started

---

## Previous Versions

### [Earlier] - Complex Wrapper Approach

Used `raylib_wrapper.h` with macro gymnastics to prevent conflicts. This worked but was overcomplicated and hard to maintain. See git history for details.

---

**Note:** This changelog follows the principles of:
- Keeping it simple (KISS)
- Following official documentation
- Trusting the library authors
- Avoiding premature optimization