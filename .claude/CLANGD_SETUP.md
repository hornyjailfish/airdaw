# Clangd Configuration Guide for AirDAW

## Problem

By default, clangd may not properly resolve raylib headers, causing it to incorrectly assume Windows GDI functions instead of raylib functions due to naming overlaps (e.g., `DrawText`, `DrawRectangle`).

This happens because:
1. Both `wingdi.h` (Windows) and `raylib.h` define similar function names AND types (e.g., `Rectangle`)
2. Without proper include paths, clangd defaults to system headers
3. Raylib headers need to be prioritized over system headers
4. Windows GDI types must be excluded using preprocessor macros

## Solution

We provide two configuration files:
1. **`.clangd`** - Direct clangd configuration
2. **`compile_commands.json`** - Compilation database (generated)

---

## Quick Setup

### Step 1: Generate compile_commands.json
```bash
just generate-compile-commands
```

### Step 2: Restart Language Server
- **VSCode**: Reload window (Ctrl+Shift+P → "Reload Window")
- **Neovim**: `:LspRestart`
- **Emacs**: Restart LSP server

### Step 3: Verify
Open `main_raylib.c` and hover over `DrawText` - it should resolve to raylib's version, not wingdi.h.

---

## Configuration Files

### `.clangd`
Located at project root. Configures clangd directly.

**Key settings:**
- **Raylib include path**: `C:/Users/5q/scoop/apps/raylib/current/include` (must be first!)
- **Vendor includes**: miniaudio, clay, sokol, ctest
- **C11 standard**: Ensures proper language features
- **Windows GDI exclusion**: `-DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX`
- **Diagnostics**: Configured for sensible warnings

**Why raylib must be first:**
```yaml
CompileFlags:
  Add:
    - -IC:/Users/5q/scoop/apps/raylib/current/include  # ← Must be first!
    - -Ivendor
    - -Ivendor/miniaudio
    - -DNOGDI           # Exclude Windows GDI types
    - -DNOUSER          # Exclude Windows USER types
    - -DWIN32_LEAN_AND_MEAN
    - -DNOMINMAX
```

This ensures raylib headers are found before system headers AND prevents Windows GDI type conflicts.

### `compile_commands.json`
Generated compilation database. Contains exact compile commands for each file.

**Generate with:**
```bash
just generate-compile-commands
```

**What it does:**
- Lists all source files in project
- Specifies exact compiler flags for each file
- Tells clangd how to compile each file
- Ensures correct include paths

---

## Editor-Specific Setup

### Visual Studio Code

1. **Install Extension**: C/C++ (Microsoft) or clangd (LLVM)

2. **If using C/C++ extension**, disable IntelliSense:
   ```json
   // .vscode/settings.json
   {
       "C_Cpp.intelliSenseEngine": "disabled",
       "clangd.path": "clangd",
       "clangd.arguments": [
           "--background-index",
           "--compile-commands-dir=${workspaceFolder}",
           "--header-insertion=never"
       ]
   }
   ```

3. **If using clangd extension**, it should work automatically

4. **Verify**:
   - Open `main_raylib.c`
   - Hover over `DrawText`
   - Should show: `void DrawText(const char *text, int posX, int posY, int fontSize, Color color)`
   - NOT: `BOOL DrawTextA(...)` (from wingdi.h)

### Neovim (with nvim-lspconfig)

```lua
-- ~/.config/nvim/lua/lsp.lua
local lspconfig = require('lspconfig')

lspconfig.clangd.setup({
    cmd = {
        "clangd",
        "--background-index",
        "--compile-commands-dir=" .. vim.fn.getcwd(),
        "--header-insertion=never",
    },
    root_dir = lspconfig.util.root_pattern(
        "compile_commands.json",
        ".clangd",
        ".git"
    ),
})
```

### Emacs (with lsp-mode)

```elisp
;; ~/.emacs.d/init.el
(use-package lsp-mode
  :hook (c-mode . lsp)
  :config
  (setq lsp-clients-clangd-args
        '("--background-index"
          "--compile-commands-dir=."
          "--header-insertion=never")))
```

### Vim (with vim-lsp)

```vim
" ~/.vimrc
if executable('clangd')
    au User lsp_setup call lsp#register_server({
        \ 'name': 'clangd',
        \ 'cmd': {server_info->['clangd', '--background-index', '--compile-commands-dir=.']},
        \ 'whitelist': ['c', 'cpp'],
        \ })
endif
```

---

## Verification

### Test 1: Raylib Functions
Open `main_raylib.c` and check:

```c
DrawText("Test", 0, 0, 20, WHITE);
//       ^ Hover here - should show raylib signature
```

**Expected:** `void DrawText(const char *text, int posX, int posY, int fontSize, Color color)`  
**Wrong:** `BOOL DrawTextA(HDC hdc, LPCSTR lpString, ...)` (wingdi.h)

### Test 2: Raylib Types
Check the `Rectangle` type:

```c
Rectangle rect = {0, 0, 100, 100};
//        ^ Hover here - should show raylib type
```

**Expected:** `typedef struct Rectangle { float x; float y; float width; float height; } Rectangle;`  
**Wrong:** `typedef struct tagRECT { LONG left; LONG top; ... } RECT, *LPRECT;` (wingdi.h)

### Test 3: Autocomplete
Type `Draw` and trigger autocomplete:

**Should see:**
- `DrawText` (raylib)
- `DrawRectangle` (raylib)
- `DrawCircle` (raylib)

**Should NOT see first:**
- `DrawTextA` (wingdi.h)
- `DrawIcon` (wingdi.h)

### Test 4: Go to Definition
Click "Go to Definition" on `DrawText`:

**Should jump to:** `C:/Users/5q/scoop/apps/raylib/current/include/raylib.h`  
**Should NOT jump to:** `C:/Windows/System32/.../wingdi.h`

---

## Troubleshooting

### Issue: Still seeing wingdi.h functions or Rectangle type clash

**Symptom:** `Rectangle` resolves to Windows `RECT` instead of raylib's `Rectangle`, or `DrawText` shows Windows GDI signature.

**Solution 1: Verify GDI exclusion macros are present**

Check that `.clangd` and `compile_commands.json` include:
```
-DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX
```

These macros prevent Windows GDI types from being defined.

**Solution 2: Regenerate compile_commands.json**
```bash
just generate-compile-commands
# Restart language server
```

**Solution 3: Check .clangd file**
Verify raylib path is first in the includes list AND GDI exclusion macros are present.

**Solution 3: Clear clangd cache**
```bash
# Windows
del %TEMP%\.cache\clangd\* /s /q

# Linux/Mac
rm -rf ~/.cache/clangd/*
```

**Solution 5: Check raylib installation**
```bash
dir C:\Users\5q\scoop\apps\raylib\current\include\raylib.h
```

If file doesn't exist, reinstall raylib:
```bash
scoop uninstall raylib
scoop install raylib
```

### Issue: Clangd not starting

**Check clangd is installed:**
```bash
clangd --version
```

**Install if missing:**
```bash
scoop install llvm
```

**VSCode: Check output panel:**
- View → Output
- Select "clangd" from dropdown
- Check for errors

### Issue: Wrong include path

**If raylib is in different location:**

1. Find raylib:
   ```bash
   where raylib.h
   # Or
   dir C:\Users\5q\scoop\apps\raylib\current\include\raylib.h
   ```

2. Update `.clangd`:
   ```yaml
   CompileFlags:
     Add:
       - -I<YOUR_RAYLIB_PATH_HERE>
   ```

3. Update `generate_compile_commands.ps1`:
   ```powershell
   $raylibInclude = "<YOUR_RAYLIB_PATH_HERE>"
   ```

4. Regenerate:
   ```bash
   just generate-compile-commands
   ```

### Issue: Autocomplete not working

**Check LSP is running:**
- VSCode: Bottom right corner should show "clangd: <filename>"
- Check for errors in output panel

**Verify files exist:**
```bash
dir .clangd
dir compile_commands.json
```

**Restart with verbose logging:**
```bash
# VSCode settings.json
{
    "clangd.arguments": [
        "--log=verbose",
        "--background-index"
    ]
}
```

Check logs in Output panel.

---

## Advanced Configuration

### Custom .clangd per file

Create `.clangd` in subdirectory for file-specific settings:

```yaml
# tests/.clangd
CompileFlags:
  Add:
    - -I../vendor/ctest
    - -I../vendor/miniaudio
```

### Exclude files from indexing

```yaml
# .clangd
Index:
  Background: Build
  Exclude:
    - vendor/*
    - dist/*
```

### Custom diagnostics

```yaml
# .clangd
Diagnostics:
  ClangTidy:
    Add:
      - bugprone-*
      - modernize-*
    Remove:
      - modernize-use-trailing-return-type
  UnusedIncludes: Strict
```

---

## How It Works

### Include Path Priority

When clangd searches for headers:

1. **Search order without .clangd:**
   ```
   System headers (C:/Windows/...) → finds wingdi.h ❌
   Windows types like Rectangle, DrawText conflict
   ```

2. **Search order with .clangd:**
   ```
   Raylib (C:/Users/.../raylib/include) → finds raylib.h ✅
   System headers → present but GDI types excluded via macros
   ```

### GDI Exclusion Macros

The following macros prevent Windows GDI types from being defined:

- **`-DNOGDI`**: Excludes `wingdi.h` definitions (Rectangle, DrawText, etc.)
- **`-DNOUSER`**: Excludes `winuser.h` definitions
- **`-DWIN32_LEAN_AND_MEAN`**: Excludes rarely-used Windows APIs
- **`-DNOMINMAX`**: Prevents `min`/`max` macro definitions

This allows raylib to define its own `Rectangle`, `DrawText`, etc. without conflicts.

### Compilation Database

`compile_commands.json` tells clangd exactly how each file is compiled:

```json
[
  {
    "directory": "C:/Users/5q/gits/airdaw",
    "command": "clang -IC:/Users/.../raylib/include main_raylib.c",
    "file": "main_raylib.c"
  }
]
```

This mirrors the actual build process.

---

## Maintenance

### When to regenerate compile_commands.json

Regenerate when you:
- Add new source files
- Change include paths
- Add new dependencies
- Update compiler flags

```bash
just generate-compile-commands
```

### When to update .clangd

Update when you:
- Change coding standards
- Add new warning suppressions
- Modify include directories

Edit `.clangd` directly, then restart LSP.

---

## FAQ

**Q: Why not use c_cpp_properties.json?**  
A: That's for VSCode's C/C++ extension. We use clangd for better accuracy and performance.

**Q: Can I use both .clangd and compile_commands.json?**  
A: Yes! They complement each other. `.clangd` provides global settings, `compile_commands.json` provides per-file specifics.

**Q: What if I use MSVC instead of clang?**  
A: Clangd still works! It parses the code independently of your build compiler.

**Q: Does this affect the actual build?**  
A: No. These files are only for the language server. Build still uses justfile/compiler.

**Q: Why is raylib include path hardcoded?**  
A: Scoop installs to predictable locations. Adjust the path if you installed raylib differently.

**Q: What do the GDI exclusion macros do?**  
A: They prevent Windows GDI headers from defining types like `Rectangle` and functions like `DrawText`, allowing raylib to use those names without conflicts.

**Q: Will these macros break Windows API usage?**  
A: No. Raylib already includes necessary Windows APIs internally. These macros only exclude GDI-specific definitions that clash with raylib.

---

## Quick Reference

```bash
# Setup
just generate-compile-commands    # Generate compilation database
code .                            # Open VSCode (restart if already open)

# Verify
# Open main_raylib.c, hover over DrawText

# Troubleshooting
just generate-compile-commands    # Regenerate
rm compile_commands.json          # Force regenerate
del %TEMP%\.cache\clangd\* /s /q  # Clear cache (Windows)

# Check installation
clangd --version                  # Check clangd
dir C:\Users\5q\scoop\apps\raylib\current\include\raylib.h  # Check raylib
```

---

## Related Commands

```bash
just check                        # Verify dependencies
just help                         # Show all commands
just raylib                       # Build raylib version
just test                         # Run tests
```

---

## Resources

- [clangd Documentation](https://clangd.llvm.org/)
- [compile_commands.json Spec](https://clang.llvm.org/docs/JSONCompilationDatabase.html)
- [Raylib Documentation](https://www.raylib.com/)

---

**Last Updated:** 2024  
**Platform:** Windows (portable to Linux/macOS with path adjustments)  
**Status:** ✅ Working configuration for AirDAW