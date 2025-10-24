# Clangd Quick Reference - AirDAW

## The Problem

Clangd resolves `DrawText` and `Rectangle` to `wingdi.h` (Windows GDI) instead of `raylib.h` because system headers are found first, causing type and function name clashes.

## Quick Fix

```bash
# Generate compile_commands.json
just generate-compile-commands

# Restart your language server
# VSCode: Ctrl+Shift+P → "Reload Window"
# Neovim: :LspRestart
```

**That's it!** ✅

---

## Verification

Open `main_raylib.c` and test these:

**Test 1: DrawText function**
```c
DrawText("Test", 0, 0, 20, WHITE);
//       ^ Hover here
```
**✅ Correct:** `void DrawText(const char *text, int posX, int posY, int fontSize, Color color)`  
**❌ Wrong:** `BOOL DrawTextA(HDC hdc, LPCSTR lpString, ...)`

**Test 2: Rectangle type**
```c
Rectangle rect = {0, 0, 100, 100};
//        ^ Hover here
```
**✅ Correct:** `typedef struct Rectangle { float x; float y; float width; float height; }`  
**❌ Wrong:** `typedef struct tagRECT { LONG left; LONG top; ... } RECT`

---

## Files Created

### `.clangd`
Configures clangd directly. Already in project root.

**Key settings:**
```yaml
CompileFlags:
  Add:
    - -IC:/Users/5q/scoop/apps/raylib/current/include  # ← Raylib FIRST!
    - -DNOGDI           # Exclude Windows GDI (prevents Rectangle clash)
    - -DNOUSER          # Exclude Windows USER types
    - -DWIN32_LEAN_AND_MEAN
    - -DNOMINMAX
```

These macros prevent Windows GDI types (`Rectangle`, `DrawText`, etc.) from being defined.

### `compile_commands.json`
Generated compilation database. Tells clangd exactly how to compile each file.

**Regenerate with:**
```bash
just generate-compile-commands
```

---

## Troubleshooting

### Still seeing wingdi.h or Rectangle type clash?

**Symptom:** `Rectangle` resolves to Windows `RECT` or `DrawText` shows Windows GDI signature.

**1. Verify GDI exclusion macros:**
Check `.clangd` contains:
```
-DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX
```

**2. Regenerate and restart:**
```bash
just generate-compile-commands
# Restart LSP
```

**3. Clear clangd cache:**
```bash
# Windows
del %TEMP%\.cache\clangd\* /s /q

# Linux/macOS
rm -rf ~/.cache/clangd/*
```

**4. Check raylib installation:**
```bash
dir C:\Users\5q\scoop\apps\raylib\current\include\raylib.h
```

If not found:
```bash
scoop install raylib
```

### Clangd not starting?

**Check it's installed:**
```bash
clangd --version
```

**Install if missing:**
```bash
scoop install llvm
```

---

## Editor Setup

### VSCode (with clangd extension)

Works automatically with `.clangd` and `compile_commands.json`.

**Optional settings.json:**
```json
{
    "clangd.arguments": [
        "--background-index",
        "--header-insertion=never"
    ]
}
```

### Neovim (nvim-lspconfig)

```lua
require('lspconfig').clangd.setup({
    cmd = { "clangd", "--background-index" }
})
```

### Other Editors

See `CLANGD_SETUP.md` for detailed configuration.

---

## Common Commands

```bash
# Setup
just generate-compile-commands    # Generate compilation DB
setup_clangd.bat                  # Run automated setup

# Verify
just check                        # Check all dependencies

# Build
just run                          # Build and run
just test                         # Run tests
```

---

## Why This Works

**Without configuration:**
```
System headers (wingdi.h) → DrawText, Rectangle from Windows GDI ❌
```

**With .clangd + compile_commands.json:**
```
Raylib headers (raylib.h) → DrawText, Rectangle from Raylib ✅
Windows GDI types excluded via macros
```

The key is:
1. **Include path priority** - Raylib searched before system headers
2. **GDI exclusion macros** - Prevents Windows GDI type definitions

---

## When to Regenerate

Regenerate `compile_commands.json` when you:
- Add new source files
- Change include paths
- Update dependencies
- Modify compiler flags

```bash
just generate-compile-commands
```

---

## Getting Help

- **Detailed guide:** `CLANGD_SETUP.md`
- **Full docs:** `README.md`
- **Clangd docs:** https://clangd.llvm.org/

---

## Quick Test

```c
// main_raylib.c

// Test 1: Function
DrawText("Hello", 0, 0, 20, WHITE);
//       ^ Hover - should show raylib signature

// Test 2: Type
Rectangle rect = {0, 0, 100, 100};
//        ^ Hover - should show raylib Rectangle struct

// ✅ Both should resolve to raylib, NOT wingdi.h
```

If both resolve to raylib, you're all set! 🎉

---

**Summary:**
1. Run: `just generate-compile-commands`
2. Restart your LSP
3. Verify `DrawText` and `Rectangle` resolve to raylib (not wingdi.h)
4. Done! ✨

**Key Fix:** GDI exclusion macros (`-DNOGDI`, etc.) prevent Windows type conflicts.