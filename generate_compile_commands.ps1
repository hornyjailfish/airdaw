# Generate compile_commands.json for clangd
# This helps the language server properly resolve raylib headers

$projectDir = $PSScriptRoot
$cc = "clang"
$cflags = "-std=c11 -Wall -Wextra -O2"
$defines = "-D_CRT_SECURE_NO_WARNINGS -DNOGDI -DWIN32_LEAN_AND_MEAN -DNOMINMAX"

# Raylib include path
$raylibInclude = "C:/Users/5q/scoop/apps/raylib/current/include"

# Create compile commands array
$commands = @(
    @{
        directory = $projectDir
        command = "$cc $cflags $defines -I$raylibInclude -Ivendor -Ivendor/miniaudio -c main_raylib.c"
        file = "main_raylib.c"
    },
    @{
        directory = $projectDir
        command = "$cc $cflags $defines -Ivendor -Ivendor/clay -Ivendor/miniaudio -Ivendor/sokol -c main.c"
        file = "main.c"
    },
    @{
        directory = "$projectDir/tests"
        command = "$cc $cflags $defines -I../vendor -I../vendor/ctest -I../vendor/miniaudio -c test_audio_engine.c"
        file = "tests/test_audio_engine.c"
    },
    @{
        directory = "$projectDir/tests"
        command = "$cc $cflags $defines -I../vendor -I../vendor/ctest -I../vendor/miniaudio -c test_audio_processing.c"
        file = "tests/test_audio_processing.c"
    },
    @{
        directory = "$projectDir/tests"
        command = "$cc $cflags $defines -I../vendor -I../vendor/ctest -I../vendor/miniaudio -c test_integration.c"
        file = "tests/test_integration.c"
    }
)

# Convert to JSON and save
$json = $commands | ConvertTo-Json -Depth 10
$json | Set-Content -Path "$projectDir/compile_commands.json" -Encoding UTF8

Write-Host "✓ Generated compile_commands.json" -ForegroundColor Green
Write-Host "  Location: $projectDir/compile_commands.json"
Write-Host "  Raylib include: $raylibInclude"
Write-Host ""
Write-Host "Restart your language server (clangd) to apply changes."
