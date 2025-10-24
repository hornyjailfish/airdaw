# Rectangle Type Clash Fix - Windows GDI vs Raylib

## The Problem

When using raylib on Windows, you may encounter a **type conflict** where the `Rectangle` type resolves to Windows GDI's `RECT` instead of raylib's `Rectangle`.

### Symptoms

1. **Incorrect type resolution in clangd:**
   ```c
   Rectangle rect = {0, 0, 100, 100};
   //        ^ Resolves to Windows RECT (LONG left, top, right, bottom)
   //        instead of raylib Rectangle (float x, y, width, height)
   ```

2. **Compilation errors:**
   ```
   error: invalid type 'RECT' for field initializer
   ```

3. **Wrong autocomplete suggestions:**
   - Shows `RECT`, `LPRECT` (Windows)
   - Instead of `Rectangle` (raylib)

4. **Similar issues with other types/functions:**
   - `DrawText` → Windows GDI instead of raylib
   - `Color` → Windows color types
   - `Vector2` → May conflict with DirectX types

## Why This Happens

### Root Cause

Both Windows GDI (`wingdi.h`) and raylib define types with the same or similar names:

| Type/Function | Windows GDI (wingdi.h) | Raylib (raylib.h) |
|---------------|------------------------|-------------------|
| **Rectangle** | `typedef struct tagRECT { LONG left; LONG top; LONG right; LONG bottom; } RECT;` | `typedef struct Rectangle { float x; float y; float width; float height; } Rectangle;` |
| **DrawText** | `BOOL DrawTextA(HDC, LPCSTR, ...)` | `void DrawText(const char*, int, int, int, Color)` |
| **Color** | Various GDI color types | `typedef struct Color { unsigned char r, g, b, a; } Color;` |

### Include Order Problem

Without proper configuration:
```
1. Compiler searches system headers first
2. Finds C:\Windows\...\wingdi.h
3. Defines Windows RECT type as "Rectangle" (via macros/typedefs)
4. When raylib.h is included later, Rectangle is already defined
5. Results in type conflict or wrong type being used
```

## The Solution

### GDI Exclusion Macros

Windows provides preprocessor macros to exclude specific header sections:

```c
#define NOGDI              // Exclude GDI types (Rectangle, DrawText, etc.)
#define NOUSER             // Exclude USER types
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used APIs
#define NOMINMAX           // Exclude min/max macros
```

These macros tell Windows headers to **skip** GDI definitions, allowing raylib to define them instead.

## How to Apply the Fix

### Method 1: Already Applied (Recommended)

The fix is **already applied** in AirDAW's configuration files:

1. **`.clangd`** - For language server (clangd)
2. **`justfile`** - For actual compilation
3. **`compile_commands.json`** - Generated with correct flags

**Just regenerate:**
```bash
just generate-compile-commands
```

Then restart your language server (VSCode: Reload Window, Neovim: `:LspRestart`).

### Method 2: Manual Application

If you need to apply this to other projects:

#### For clangd (.clangd file):
```yaml
CompileFlags:
  Add:
    - -IC:/path/to/raylib/include  # Raylib FIRST!
    - -DNOGDI                       # Exclude Windows GDI
    - -DNOUSER                      # Exclude Windows USER
    - -DWIN32_LEAN_AND_MEAN         # Lean Windows
    - -DNOMINMAX                    # No min/max macros
    - -std=c11
```

#### For compilation (justfile/Makefile/CMake):
```bash
# Justfile
DEFINES := "-DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX"

# Makefile
CFLAGS += -DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX

# CMakeLists.txt
add_compile_definitions(NOGDI NOUSER WIN32_LEAN_AND_MEAN NOMINMAX)
```

#### In source code (before any includes):
```c
// At the very top of your file, before ANY includes
#define NOGDI
#define NOUSER
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include "raylib.h"
// ... other includes
```

## Verification Steps

### Step 1: Check Type Resolution

Open `main_raylib.c` in your editor and hover over `Rectangle`:

```c
Rectangle rect = {0, 0, 100, 100};
//        ^ Hover here
```

**✅ Correct (raylib):**
```
typedef struct Rectangle {
    float x;        // Rectangle top-left corner position x
    float y;        // Rectangle top-left corner position y
    float width;    // Rectangle width
    float height;   // Rectangle height
} Rectangle;
```

**❌ Wrong (Windows GDI):**
```
typedef struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} RECT, *PRECT, *LPRECT;
```

### Step 2: Check Function Resolution

Hover over `DrawText`:

```c
DrawText("Test", 0, 0, 20, WHITE);
//       ^ Hover here
```

**✅ Correct (raylib):**
```
void DrawText(const char *text, int posX, int posY, int fontSize, Color color);
```

**❌ Wrong (Windows GDI):**
```
BOOL DrawTextA(HDC hdc, LPCSTR lpString, int nCount, LPRECT lpRect, UINT format);
```

### Step 3: Test Compilation

```bash
just raylib
```

Should compile without type-related errors.

### Step 4: Test Autocomplete

Type `Rect` and trigger autocomplete:

**Should see:**
- `Rectangle` (raylib struct)

**Should NOT see first:**
- `RECT` (Windows)
- `LPRECT` (Windows pointer)

## Technical Details

### What Each Macro Does

#### NOGDI
```c
#define NOGDI
```
- **Excludes:** `wingdi.h` (Windows GDI)
- **Prevents:** Rectangle, DrawText, Polygon, Color types/functions
- **Effect:** ~50KB of Windows GDI definitions excluded

#### NOUSER
```c
#define NOUSER
```
- **Excludes:** `winuser.h` (Windows USER)
- **Prevents:** Button, MessageBox, Window management
- **Effect:** ~30KB of Windows USER definitions excluded

#### WIN32_LEAN_AND_MEAN
```c
#define WIN32_LEAN_AND_MEAN
```
- **Excludes:** Rarely-used Windows APIs
- **Keeps:** Core functionality (CreateFile, CreateThread, etc.)
- **Effect:** Faster compilation, smaller binaries

#### NOMINMAX
```c
#define NOMINMAX
```
- **Prevents:** `#define min(a,b) ...` and `#define max(a,b) ...`
- **Why:** These macros can interfere with C++ `std::min`/`std::max`
- **Effect:** Avoids macro pollution

### Include Order Matters

**Wrong order (Windows GDI wins):**
```c
#include <windows.h>  // Defines RECT as "Rectangle"
#include "raylib.h"   // Too late, Rectangle already defined
```

**Correct order (raylib wins):**
```c
#define NOGDI
#define NOUSER
#include "raylib.h"   // Defines Rectangle first
// windows.h can be included later if needed, but GDI types are excluded
```

### Memory Layout Comparison

#### Raylib Rectangle (16 bytes):
```
┌─────────┬─────────┬──────────┬───────────┐
│ x (4B)  │ y (4B)  │ width(4B)│ height(4B)│
│ float   │ float   │ float    │ float     │
└─────────┴─────────┴──────────┴───────────┘
```

#### Windows RECT (16 bytes):
```
┌────────┬────────┬─────────┬──────────┐
│left(4B)│top (4B)│right(4B)│bottom(4B)│
│ LONG   │ LONG   │ LONG    │ LONG     │
└────────┴────────┴─────────┴──────────┘
```

**Note:** Different semantics!
- Raylib: `(x, y, width, height)` - position + size
- Windows: `(left, top, right, bottom)` - two corners

## Troubleshooting

### Issue 1: Still seeing RECT instead of Rectangle

**Cause:** Macros not defined or defined too late.

**Solution:**
```bash
# Regenerate compile_commands.json
just generate-compile-commands

# Clear clangd cache
del %TEMP%\.cache\clangd\* /s /q  # Windows
rm -rf ~/.cache/clangd/*          # Linux/macOS

# Restart language server
```

### Issue 2: Compilation fails with "undefined reference to Rectangle"

**Cause:** Raylib not linked properly.

**Solution:**
```bash
# Check raylib installation
dir C:\Users\5q\scoop\apps\raylib\current\include\raylib.h

# Reinstall if needed
scoop uninstall raylib
scoop install raylib
```

### Issue 3: Type mismatch errors

**Error:**
```
error: incompatible types when initializing 'RECT'
```

**Solution:**
Ensure GDI exclusion macros are in **all** relevant places:
- `.clangd` file
- `justfile` DEFINES variable
- `compile_commands.json` (regenerate)
- Source files (if manually including headers)

### Issue 4: Autocomplete shows both RECT and Rectangle

**Cause:** Partial configuration or stale cache.

**Solution:**
1. Verify `.clangd` has ALL macros
2. Regenerate: `just generate-compile-commands`
3. Clear cache: `del %TEMP%\.cache\clangd\* /s /q`
4. Restart language server
5. Wait 30 seconds for re-indexing

## Impact on Other Code

### Does this break Windows API usage?

**No.** Raylib internally includes necessary Windows APIs. These macros only exclude GDI-specific types that clash.

**You can still use:**
- `CreateFile()`, `ReadFile()`, `WriteFile()` - File I/O
- `CreateThread()`, `WaitForSingleObject()` - Threading
- `GetSystemTime()`, `Sleep()` - System utilities
- `VirtualAlloc()` - Memory management

**You cannot directly use (but don't need to):**
- `CreateDC()`, `GetDC()` - GDI device contexts
- `BitBlt()`, `StretchBlt()` - GDI blitting (use raylib's drawing)
- Windows drawing functions (use raylib equivalents)

### Does this affect miniaudio or other libraries?

**No.** These libraries use lower-level Windows APIs (WASAPI, etc.) that are not affected by GDI exclusion.

## Best Practices

1. **Always define macros before including headers:**
   ```c
   #define NOGDI
   #define NOUSER
   #include "raylib.h"
   ```

2. **Use in compilation flags for consistency:**
   ```bash
   -DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX
   ```

3. **Document the requirement:**
   ```c
   // NOTE: NOGDI is required to prevent Rectangle type clash with Windows GDI
   #define NOGDI
   ```

4. **Keep raylib includes first:**
   ```c
   #include "raylib.h"    // First!
   #include "miniaudio.h"
   #include <stdio.h>
   // Other includes...
   ```

## Summary

### The Fix (One-Liner)

Add these compiler flags:
```bash
-DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX
```

### What It Does

1. **Prevents** Windows GDI type definitions
2. **Allows** raylib to define Rectangle, DrawText, etc.
3. **Resolves** type conflicts in both compilation and IDE

### How to Verify

1. Hover over `Rectangle` → Should show raylib struct
2. Hover over `DrawText` → Should show raylib function
3. Compile → No type mismatch errors
4. Autocomplete → Shows raylib types first

### Already Applied In

- ✅ `.clangd` configuration
- ✅ `justfile` DEFINES variable
- ✅ `compile_commands.json` (when generated)
- ✅ All build commands

**Just run:** `just generate-compile-commands` and restart your language server!

---

**Status:** ✅ Fixed in AirDAW  
**Applies to:** Windows builds only  
**Side effects:** None - Windows API still accessible  
**Performance:** Slightly faster compilation (fewer headers parsed)

---

## Quick Reference

```bash
# Apply fix
just generate-compile-commands

# Verify in editor
# Hover over Rectangle → should show raylib type

# Test build
just raylib

# If issues persist
del %TEMP%\.cache\clangd\* /s /q  # Clear cache
# Restart language server
```

**Documentation:** See `CLANGD_SETUP.md` for full details.