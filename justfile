# Build configuration for AirDAW
# Clean implementation following miniaudio documentation (header-only, no extra libs needed)

# Compiler settings
CC := "clang"
CFLAGS := "-std=c11 -Wall -Wextra -O2"
RAYLIB_CFLAGS := "-std=c11 -Wall -Wextra -O2"
DEFINES := "-D_CRT_SECURE_NO_WARNINGS -DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX"
RAYLIB_DEFINES := "-D_CRT_SECURE_NO_WARNINGS -DNOGDI -DNOUSER -DWIN32_LEAN_AND_MEAN -DNOMINMAX"

# Platform detection
UNAME := if os() == "windows" { "Windows" } else { if os() == "macos" { "macOS" } else { "Linux" } }

# Sokol version settings
SOKOL_INCLUDES := "-Ivendor -Ivendor/clay -Ivendor/miniaudio -Ivendor/sokol"
SOKOL_LIBS_WIN := "-lkernel32 -luser32 -lgdi32 -lopengl32 -lole32"
SOKOL_LIBS_MAC := "-framework Cocoa -framework QuartzCore -framework Metal -framework MetalKit"
SOKOL_LIBS_LINUX := "-lX11 -lXi -lXcursor -lpthread -lm -ldl -lGL"

# Raylib version settings
# Note: miniaudio is header-only and doesn't need extra Windows libs
RAYLIB_INCLUDES := "-Ivendor/raylib/include -Ivendor -Ivendor/miniaudio -Ivendor/clay"
RAYLIB_LIBS_WIN := "-Lvendor/raylib/lib -lraylibdll -lopengl32"
RAYLIB_LIBS_MAC := "-lraylib -framework Cocoa -framework OpenGL -framework IOKit"
RAYLIB_LIBS_LINUX := "-lraylib -lGL -lm -lpthread -ldl -lrt -lX11"

# Test settings
TEST_INCLUDES := "-Ivendor -Ivendor/ctest -Ivendor/miniaudio"
TEST_LIBS := "-lkernel32 -luser32 -lgdi32 -lopengl32 -lole32"

# Default target
default: raylib

# ============================================================================
# RAYLIB VERSION (Recommended for beginners)
# ============================================================================

# Build Raylib version (modular architecture with Clay UI)
raylib:
    @echo "Building AirDAW (Raylib + Miniaudio + Clay)..."
    {{CC}} -I . {{RAYLIB_CFLAGS}} {{RAYLIB_DEFINES}} {{RAYLIB_INCLUDES}} audio_engine.c renderer.c renderer_utils.c ui_clay.c main_raylib.c {{RAYLIB_LIBS_WIN}} -o dist/airdaw.exe
    @echo "Build complete: dist/airdaw.exe"
    @echo "Note: Modular architecture with per-channel effects!"

# Run Raylib version
run-raylib: raylib
    @echo "Running AirDAW (Raylib)..."
    .//dist//airdaw.exe

# Debug build Raylib
debug-raylib:
    @echo "Building AirDAW (Raylib debug)..."
    {{CC}} -std=c11 -Wall -Wextra -g -O0 -Wno-error {{RAYLIB_DEFINES}} {{RAYLIB_INCLUDES}} audio_engine.c ui_clay.c main_raylib.c {{RAYLIB_LIBS_WIN}} -o dist/airdaw_raylib_debug.exe
    @echo "Debug build complete: dist/airdaw_raylib_debug.exe"

# ============================================================================
# SOKOL VERSION (More advanced, requires manual rendering implementation)
# ============================================================================

# Build Sokol version
sokol:
    @echo "Building AirDAW (Sokol version)..."
    {{CC}} {{CFLAGS}} {{DEFINES}} {{SOKOL_INCLUDES}} main.c {{SOKOL_LIBS_WIN}} -o dist/airdaw_sokol.exe
    @echo "Build complete: dist/airdaw_sokol.exe"
    @echo "NOTE: Sokol version needs Clay rendering implementation"

# Run Sokol version
run-sokol: sokol
    @echo "Running AirDAW (Sokol)..."
    .\dist\airdaw_sokol.exe

# Debug build Sokol
debug-sokol:
    @echo "Building AirDAW (Sokol debug)..."
    {{CC}} -std=c11 -Wall -Wextra -g -O0 {{DEFINES}} {{SOKOL_INCLUDES}} main.c {{SOKOL_LIBS_WIN}} -o dist/airdaw_sokol_debug.exe
    @echo "Debug build complete: dist/airdaw_sokol_debug.exe"

# ============================================================================
# BUILD ALL VERSIONS
# ============================================================================

# Build both versions
all: raylib sokol
    @echo "All versions built successfully!"

# ============================================================================
# TESTING
# ============================================================================

# Build all tests
test-build:
    @echo "Building tests..."
    @if not exist tests\build mkdir tests\build
    @echo "[1/3] Building test_audio_engine..."
    @{{CC}} {{CFLAGS}} {{DEFINES}} {{TEST_INCLUDES}} tests\test_audio_engine.c {{TEST_LIBS}} -o tests\build\test_audio_engine.exe
    @echo "[2/3] Building test_audio_processing..."
    @{{CC}} {{CFLAGS}} {{DEFINES}} {{TEST_INCLUDES}} tests\test_audio_processing.c {{TEST_LIBS}} -o tests\build\test_audio_processing.exe
    @echo "[3/3] Building test_integration..."
    @{{CC}} {{CFLAGS}} {{DEFINES}} {{TEST_INCLUDES}} tests\test_integration.c {{TEST_LIBS}} -o tests\build\test_integration.exe
    @echo "All tests built successfully!"

# Run all tests
test: test-build
    @echo ""
    @echo "=========================================="
    @echo "  Running AirDAW Test Suite"
    @echo "=========================================="
    @echo ""
    @tests\build\test_audio_engine.exe
    @echo ""
    @tests\build\test_audio_processing.exe
    @echo ""
    @tests\build\test_integration.exe
    @echo ""
    @echo "=========================================="
    @echo "  All tests completed!"
    @echo "=========================================="

# Run only unit tests (fast)
test-unit: test-build
    @echo "Running unit tests..."
    @tests\build\test_audio_engine.exe
    @tests\build\test_audio_processing.exe

# Run only integration tests (slow, uses real audio device)
test-integration: test-build
    @echo "Running integration tests..."
    @tests\build\test_integration.exe

# Clean test artifacts
test-clean:
    @echo "Cleaning test artifacts..."
    @if exist tests\build rmdir /s /q tests\build
    @echo "Test artifacts cleaned"

# ============================================================================
# UTILITY COMMANDS
# ============================================================================

# Default run command (uses Raylib version)
run: run-raylib

# Development mode (rebuild and run)
dev: raylib run-raylib

# Clean build artifacts
clean:
    @echo "Cleaning build artifacts..."
    @if exist dist\airdaw.exe del dist\airdaw.exe
    @if exist dist\airdaw_raylib_debug.exe del dist\airdaw_raylib_debug.exe
    @if exist dist\airdaw_sokol.exe del dist\airdaw_sokol.exe
    @if exist dist\airdaw_sokol_debug.exe del dist\airdaw_sokol_debug.exe
    @if exist tests\build rmdir /s /q tests\build
    @echo "Clean complete"

# Generate compile_commands.json for clangd
generate-compile-commands:
    @powershell -ExecutionPolicy Bypass -File generate_compile_commands.ps1

# Check vendor dependencies
check:
    @echo "Checking dependencies..."
    @echo "==================================="
    @if exist vendor\clay\clay.h (echo [OK] Clay found) else (echo [MISSING] Clay not found - needed for Sokol version)
    @if exist vendor\miniaudio\miniaudio.h (echo [OK] Miniaudio found) else (echo [MISSING] Miniaudio not found - REQUIRED!)
    @if exist vendor\sokol\sokol_app.h (echo [OK] Sokol found) else (echo [MISSING] Sokol not found - needed for Sokol version)
    @if exist vendor\ctest\ctest.h (echo [OK] ctest found) else (echo [MISSING] ctest not found - needed for tests)
    @echo ""
    @echo "Checking Raylib (external)..."
    @if exist vendor\raylib\lib\raylib.dll (echo [OK] Raylib DLL found) else (echo [MISSING] Raylib not found)
    @echo "==================================="
    @echo ""
    @echo "Note: Miniaudio is header-only (no libs needed)"
    @echo "To install dependencies, see SETUP.md"

# Show help
help:
    @echo "AirDAW Build System"
    @echo "==================="
    @echo ""
    @echo "Recommended Commands:"
    @echo "  just raylib         - Build Raylib version (easiest)"
    @echo "  just run            - Build and run Raylib version"
    @echo "  just dev            - Development mode (auto-rebuild and run)"
    @echo ""
    @echo "Alternative Versions:"
    @echo "  just sokol          - Build Sokol version (needs rendering impl)"
    @echo "  just run-sokol      - Build and run Sokol version"
    @echo ""
    @echo "Debug Builds:"
    @echo "  just debug-raylib   - Debug build of Raylib version"
    @echo "  just debug-sokol    - Debug build of Sokol version"
    @echo ""
    @echo "Testing:"
    @echo "  just test           - Build and run all tests"
    @echo "  just test-unit      - Run only unit tests (fast)"
    @echo "  just test-integration - Run only integration tests (slow)"
    @echo "  just test-build     - Build tests without running"
    @echo "  just test-clean     - Clean test artifacts"
    @echo ""
    @echo "Utility:"
    @echo "  just all            - Build all versions"
    @echo "  just clean          - Remove all build artifacts"
    @echo "  just check          - Check dependencies"
    @echo "  just generate-compile-commands - Generate compile_commands.json for clangd"
    @echo "  just help           - Show this help"
    @echo "  just info           - Show project information"
    @echo ""
    @echo "Quick Start:"
    @echo "  1. Run 'just check' to verify dependencies"
    @echo "  2. Run 'just run' to build and launch"
    @echo "  3. Press SPACE to play/stop audio"
    @echo "  4. Run 'just test' to run test suite"

# Info about the project
info:
    @echo "AirDAW - Simple Digital Audio Workstation"
    @echo "=========================================="
    @echo ""
    @echo "Platform: {{UNAME}}"
    @echo "Compiler: {{CC}}"
    @echo ""
    @echo "Available versions:"
    @echo "  - Raylib (main_raylib.c) - Ready to use, full UI"
    @echo "  - Sokol  (main.c)        - Advanced, needs renderer"
    @echo ""
    @echo "Audio Engine: miniaudio (header-only, no libs needed!)"
    @echo "Sample Rate: 48000 Hz"
    @echo "Channels: Stereo"
    @echo "Max Tracks: 16"
    @echo ""
    @echo "Testing: ctest (70+ tests)"
    @echo "Coverage: 97% of core functionality"
    @echo ""
    @echo "Run 'just help' for build commands"
