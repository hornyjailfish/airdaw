@echo off
echo ========================================
echo   AirDAW - Clangd Setup Script
echo ========================================
echo.

echo [1/3] Checking .clangd configuration...
if exist .clangd (
    echo [OK] .clangd file found
) else (
    echo [WARNING] .clangd file not found!
    echo Please ensure .clangd exists in project root
)
echo.

echo [2/3] Generating compile_commands.json...
powershell -ExecutionPolicy Bypass -File generate_compile_commands.ps1
if %ERRORLEVEL% EQU 0 (
    echo [OK] compile_commands.json generated
) else (
    echo [ERROR] Failed to generate compile_commands.json
    echo Check that generate_compile_commands.ps1 exists
    pause
    exit /b 1
)
echo.

echo [3/3] Verifying Raylib installation...
set RAYLIB_PATH=vendor\raylib\include\raylib.h
if exist "%RAYLIB_PATH%" (
    echo [OK] Raylib found at %RAYLIB_PATH%
) else (
    echo [WARNING] Raylib not found at expected location
    echo Expected: %RAYLIB_PATH%
    echo Install with: scoop install raylib
)
echo.

echo ========================================
echo   Setup Complete!
echo ========================================
echo.
echo Next steps:
echo   1. Restart your language server (clangd)
echo      - VSCode: Ctrl+Shift+P ^> "Reload Window"
echo      - Neovim: :LspRestart
echo.
echo   2. Open main_raylib.c and verify:
echo      - Hover over DrawText
echo      - Should show raylib signature
echo      - NOT wingdi.h (Windows GDI)
echo.
echo   3. If still having issues:
echo      - Clear clangd cache
echo      - Check CLANGD_SETUP.md for troubleshooting
echo.

pause
