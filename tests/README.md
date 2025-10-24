# AirDAW Test Suite

Comprehensive test suite for the AirDAW audio engine using ctest.

## Overview

The test suite covers three main areas:
1. **Audio Engine** - Unit tests for engine initialization and management
2. **Audio Processing** - Tests for mixing, panning, and audio manipulation
3. **Integration** - Full system tests with real audio device

## Test Files

### `test_audio_engine.c`
Unit tests for the audio engine core functionality.

**Tests:**
- ✅ Engine initialization and shutdown
- ✅ Track management (add, remove, limits)
- ✅ Volume control (track and master)
- ✅ Pan control
- ✅ Mute/Solo functionality
- ✅ Playback state management
- ✅ Audio buffer allocation
- ✅ Thread safety (atomic operations)
- ✅ Peak metering initialization

**Total Tests:** 30+

### `test_audio_processing.c`
Tests for audio processing algorithms and mixing.

**Tests:**
- ✅ Basic audio processing (silence, playback)
- ✅ Volume scaling (track and master)
- ✅ Panning algorithms (left, right, center)
- ✅ Mute behavior (track and master)
- ✅ Solo isolation
- ✅ Multi-track mixing
- ✅ Peak metering accuracy
- ✅ Buffer wrapping and looping
- ✅ Clipping prevention

**Total Tests:** 25+

### `test_integration.c`
Full system integration tests with real audio device.

**Tests:**
- ✅ Complete init/shutdown cycles
- ✅ Add tracks and play
- ✅ Transport control (play/stop)
- ✅ Real-time peak metering updates
- ✅ Mute during playback
- ✅ Solo isolation in real-time
- ✅ Master volume control
- ✅ Multiple track mixing
- ✅ Rapid state changes
- ✅ Playback position tracking
- ✅ Maximum tracks stress test
- ✅ Concurrent UI/audio thread operations
- ✅ Audio callback performance
- ✅ Engine restart
- ✅ Memory cleanup

**Total Tests:** 15+

**Note:** Integration tests use a real audio device and may take longer to run.

## Running Tests

### Windows

#### Option 1: Using Just (Recommended)
```bash
just test
```

#### Option 2: Manual execution
```bash
cd tests

# Build tests
clang -std=c11 -Wall -Wextra -O2 -I../vendor -I../vendor/ctest -I../vendor/miniaudio test_audio_engine.c -lkernel32 -luser32 -lgdi32 -lopengl32 -lole32 -o build/test_audio_engine.exe

# Run tests
build\test_audio_engine.exe
```

### Linux/macOS

```bash
cd tests

# Build
clang -std=c11 -Wall -Wextra -O2 -I../vendor -I../vendor/ctest -I../vendor/miniaudio test_audio_engine.c -lpthread -lm -ldl -o build/test_audio_engine

# Run
./build/test_audio_engine
```

## Test Output

### Successful Run
```
==========================================
  AirDAW Test Suite
==========================================

[OK] audio_engine::init_and_shutdown
[OK] audio_engine::initial_state
[OK] tracks::add_single_track
...

==========================================
  Test Summary
==========================================
Total Test Suites: 3
Passed: 3
Failed: 0
==========================================

[SUCCESS] All tests passed!
```

### Failed Test
```
[ERR] tracks::add_single_track: ASSERT_EQUAL failed
  Expected: 0
  Got: -1
  At: test_audio_engine.c:245

FAILED: 1 test(s) failed
```

## Test Structure

Each test file follows this pattern:

```c
#define CTEST_MAIN
#define CTEST_COLOR_OK
#include "../vendor/ctest/ctest.h"

// Test structures and helpers
typedef struct { ... } TestData;

// Test suite
CTEST(suite_name, test_name) {
    // Arrange
    TestData data;
    init_test_data(&data);
    
    // Act
    int result = function_under_test(&data);
    
    // Assert
    ASSERT_EQUAL(expected, result);
    
    // Cleanup
    cleanup_test_data(&data);
}

// Main
int main(int argc, const char* argv[]) {
    return ctest_main(argc, argv);
}
```

## Available Assertions

### Equality
- `ASSERT_EQUAL(exp, real)` - Integer equality
- `ASSERT_NOT_EQUAL(exp, real)` - Integer inequality
- `ASSERT_EQUAL_U(exp, real)` - Unsigned integer equality
- `ASSERT_STR(exp, real)` - String equality
- `ASSERT_DBL_NEAR(exp, real)` - Double equality (with epsilon)
- `ASSERT_DBL_NEAR_TOL(exp, real, tol)` - Double equality (custom tolerance)

### Comparison
- `ASSERT_LT(v1, v2)` - Less than
- `ASSERT_LE(v1, v2)` - Less than or equal
- `ASSERT_GT(v1, v2)` - Greater than
- `ASSERT_GE(v1, v2)` - Greater than or equal

### Boolean
- `ASSERT_TRUE(expr)` - Expression is true
- `ASSERT_FALSE(expr)` - Expression is false

### Null Checks
- `ASSERT_NULL(ptr)` - Pointer is NULL
- `ASSERT_NOT_NULL(ptr)` - Pointer is not NULL

### Manual Failure
- `ASSERT_FAIL()` - Force test failure

## Adding New Tests

### 1. Create Test Suite
```c
CTEST(new_feature, basic_test) {
    // Your test code
    ASSERT_TRUE(1 + 1 == 2);
}
```

### 2. Group Related Tests
```c
CTEST(feature, test1) { /* ... */ }
CTEST(feature, test2) { /* ... */ }
CTEST(feature, test3) { /* ... */ }
```

### 3. Use Setup/Teardown (Optional)
```c
CTEST_DATA(feature) {
    int value;
    float* buffer;
};

CTEST_SETUP(feature) {
    data->value = 42;
    data->buffer = malloc(1024 * sizeof(float));
}

CTEST_TEARDOWN(feature) {
    free(data->buffer);
}

CTEST2(feature, test_with_data) {
    ASSERT_EQUAL(42, data->value);
    ASSERT_NOT_NULL(data->buffer);
}
```

## Continuous Integration

Tests can be integrated into CI/CD pipelines:

```yaml
# Example GitHub Actions
- name: Run Tests
  run: |
    cd tests
    ./run_tests.bat
```

## Test Coverage

Current coverage areas:
- ✅ Audio engine initialization
- ✅ Track management
- ✅ Volume and pan control
- ✅ Mute/solo behavior
- ✅ Audio mixing algorithms
- ✅ Peak metering
- ✅ Thread safety
- ✅ Real-time performance
- ⚠️ File I/O (not yet implemented)
- ⚠️ Plugin hosting (not yet implemented)
- ⚠️ MIDI (not yet implemented)

## Troubleshooting

### "Cannot find miniaudio.h"
```bash
# Download miniaudio
curl -L https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h -o ../vendor/miniaudio/miniaudio.h
```

### "Audio device initialization failed"
- Close other audio applications
- Check audio device isn't in exclusive mode
- Run integration tests separately: `build\test_integration.exe`

### Tests hang or timeout
- Integration tests use real audio device and take time
- Some tests use `sleep_ms()` to allow audio callbacks
- Expected runtime: 10-30 seconds per test suite

### Build errors
```bash
# Check compiler version
clang --version

# Verify ctest header
dir ..\vendor\ctest\ctest.h

# Check includes
dir ..\vendor\miniaudio\miniaudio.h
```

## Performance Benchmarks

Expected test timings (on modern hardware):
- **test_audio_engine**: < 1 second
- **test_audio_processing**: < 2 seconds  
- **test_integration**: 5-10 seconds (uses real audio device)

## Best Practices

1. **Keep tests independent** - Each test should work in isolation
2. **Clean up resources** - Always free allocated memory
3. **Test edge cases** - Test limits (0, max, negative values)
4. **Use descriptive names** - `test_volume_scaling` not `test1`
5. **Add comments** - Explain complex test logic
6. **Test failures too** - Verify error handling works
7. **Avoid timing dependencies** - Use atomics and signals, not arbitrary sleeps

## Contributing

When adding features to AirDAW:

1. ✅ Write tests FIRST (TDD approach)
2. ✅ Ensure tests cover new functionality
3. ✅ Run all tests before committing
4. ✅ Update this README if adding new test files

## References

- [ctest documentation](https://github.com/bvdberg/ctest)
- [miniaudio documentation](https://miniaud.io/docs/manual/index.html)
- [Test-Driven Development](https://en.wikipedia.org/wiki/Test-driven_development)

---

**Last Updated:** 2024  
**Test Framework:** ctest  
**Total Tests:** 70+  
**Coverage:** Core audio engine and processing