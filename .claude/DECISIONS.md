# Architectural Decisions for AirDAW

This document records the key decisions made during the design and implementation of AirDAW.

## Date: 2024
**Status**: Initial implementation complete  
**Authors**: Architecture design for simple, extensible DAW engine

---

## Decision 1: Use C (C11) as Primary Language

### Context
Need to choose a programming language for a DAW with real-time audio requirements.

### Options Considered
1. **C** - Low-level, explicit control
2. **C++** - More features, but complexity
3. **Rust** - Memory safety, modern
4. **C#** - High-level, GC

### Decision: **C (C11 standard)**

### Rationale
- **Real-time friendly**: No hidden allocations, no garbage collection
- **Explicit control**: Clear memory management, no surprises
- **Universal**: Works everywhere, easy to integrate with other systems
- **Simple**: Less cognitive overhead than C++
- **Performance**: Direct hardware access, minimal runtime
- **Atomics**: C11 provides `stdatomic.h` for lock-free programming

### Trade-offs
- ❌ More manual memory management
- ❌ No RAII or modern conveniences
- ✅ Full control over performance-critical code
- ✅ Easier to reason about real-time behavior

---

## Decision 2: Use miniaudio for Audio Engine

### Context
Need cross-platform audio I/O with low latency and simple API.

### Options Considered
1. **miniaudio** - Single header, simple
2. **PortAudio** - Mature, widely used
3. **RtAudio** - C++, flexible
4. **Platform-specific APIs** - WASAPI, CoreAudio, ALSA

### Decision: **miniaudio**

### Rationale
- **Single header**: No build system complexity
- **Cross-platform**: Windows, macOS, Linux, mobile, web
- **Low latency**: Direct access to native APIs (WASAPI, CoreAudio, ALSA)
- **Simple API**: Easy to understand and use
- **No dependencies**: Completely self-contained
- **Battle-tested**: Used in many production applications
- **Active maintenance**: Well-supported project

### Trade-offs
- ❌ Less control than raw platform APIs
- ✅ 95% of features needed for a DAW
- ✅ Significantly faster development time

**Code Impact**:
```c
ma_device_config config = ma_device_config_init(ma_device_type_playback);
config.dataCallback = audio_callback;
ma_device_init(NULL, &config, &device);
ma_device_start(&device);
```

---

## Decision 3: Use Clay for UI Layout

### Context
Need a flexible layout system for complex DAW UI (mixer, tracks, timeline).

### Options Considered
1. **Manual layout** - Calculate positions manually
2. **Clay** - Declarative layout library
3. **Yoga** - Flexbox layout (Facebook)
4. **ImGui-style** - Immediate mode

### Decision: **Clay (for Sokol version)**

### Rationale
- **Declarative**: CSS Flexbox-like API, easy to reason about
- **Renderer-agnostic**: Separates layout from rendering
- **Modern**: Container-based layout system
- **Performance**: Fast layout calculation
- **Clean code**: Reduces UI spaghetti

### Trade-offs
- ❌ Requires manual rendering implementation
- ✅ Scales to complex UIs
- ✅ Professional-grade architecture

**Code Example**:
```c
CLAY(CLAY_ID("Track"),
     CLAY_LAYOUT(.sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(120)},
                 .childGap = 8)) {
    CLAY_TEXT("Track 1", ...) {}
    // Buttons, sliders automatically laid out
}
```

---

## Decision 4: Provide Two UI Engine Options

### Context
Users have different needs: beginners want simplicity, experts want control.

### Options Considered
1. **Raylib only** - Easy but less scalable
2. **Sokol + Clay only** - Powerful but complex
3. **Both options** - More work upfront, flexibility later

### Decision: **Provide both implementations**

### Rationale
- **Beginners**: Can start with Raylib (`main_raylib.c`)
  - Working UI out of the box
  - Immediate results
  - Easy to learn
  
- **Advanced users**: Can use Sokol + Clay (`main.c`)
  - Maximum performance
  - Scalable architecture
  - Full customization

- **Learning path**: Start with Raylib, migrate to Sokol if needed

### Trade-offs
- ❌ Two codebases to maintain (initially)
- ✅ Accommodates all skill levels
- ✅ Demonstrates both approaches
- ✅ Educational value

---

## Decision 5: Multithreaded Architecture

### Context
Audio processing cannot block on UI updates, UI cannot wait for audio.

### Options Considered
1. **Single-threaded** - Simple but limited
2. **Audio + UI threads** - Standard approach
3. **Audio + UI + I/O threads** - Full separation

### Decision: **Audio thread + Main thread (with optional I/O thread)**

### Architecture
```
Audio Thread (High Priority)
  - Real-time audio callback
  - No locks, no allocations
  - Lock-free communication via atomics

Main Thread (Normal Priority)
  - UI rendering (60fps)
  - User input handling
  - Can use locks and allocations

I/O Thread (Optional, Low Priority)
  - File loading/saving
  - Background processing
```

### Rationale
- **Prevents audio dropouts**: UI never blocks audio
- **Responsive UI**: Audio processing doesn't slow down UI
- **Lock-free**: Atomics for shared state (mute, solo, volume)
- **Simple**: Only 2 threads required initially
- **Scalable**: Can add I/O thread when needed

### Trade-offs
- ❌ More complex than single-threaded
- ✅ Professional-grade architecture
- ✅ No audio glitches from UI operations

**Implementation**:
```c
// Shared state
atomic_bool muted;
atomic_int peak_left;
atomic_bool is_playing;

// UI thread writes, audio thread reads
atomic_store(&muted, true);

// Audio thread reads
bool is_muted = atomic_load(&muted);
```

---

## Decision 6: Lock-Free Communication

### Context
How to share state between audio and UI threads safely.

### Options Considered
1. **Mutexes/Locks** - Simple but dangerous
2. **Atomics** - Lock-free, safe
3. **Message passing** - Clean but complex

### Decision: **Atomics for simple state, message passing for complex operations**

### Rationale
- **Atomics**: Perfect for booleans, integers, floats
  - No priority inversion
  - No blocking
  - Real-time safe
  
- **Message queues**: For complex state changes
  - Queue commands from UI to audio
  - Process in audio thread's time
  
### What Uses Atomics
- `atomic_bool muted, solo, playing`
- `atomic_int peak_left, peak_right`
- `atomic_uint playback_position`

### What Could Use Message Queues (Future)
- Plugin parameter changes
- Automation events
- Effect chain updates

---

## Decision 7: Pre-allocate Audio Buffers

### Context
Cannot allocate memory in real-time audio callback.

### Decision: **Allocate all buffers at initialization**

### Rationale
- `malloc()` is unpredictable (can take milliseconds)
- Audio callback must be deterministic
- Pre-allocation ensures no runtime allocations

**Implementation**:
```c
bool audio_engine_init(AudioEngine* engine) {
    // Allocate once, use forever
    for (int i = 0; i < MAX_TRACKS; i++) {
        tracks[i].audio_data = malloc(MAX_SAMPLES * sizeof(float));
    }
}

void audio_callback(...) {
    // NO malloc() allowed here!
    // Use pre-allocated buffers only
}
```

---

## Decision 8: Simple Build System (Just)

### Context
Need build system that's easy to use and cross-platform.

### Options Considered
1. **Make** - Universal but arcane syntax
2. **CMake** - Powerful but complex
3. **Just** - Modern, simple
4. **Shell scripts** - Platform-specific

### Decision: **Just command runner**

### Rationale
- **Simple syntax**: Easy to read and modify
- **Fast**: No configure step
- **Cross-platform**: Works on Windows, macOS, Linux
- **No hidden magic**: Clear what each command does
- **Modern**: Better DX than Make

**Example**:
```bash
just run      # Build and run
just clean    # Clean build
just check    # Verify dependencies
```

---

## Decision 9: Header-Only Dependencies

### Context
How to manage external libraries.

### Decision: **Use header-only libraries in `vendor/` directory**

### Rationale
- **No build complexity**: Just include the header
- **Version control**: Can track specific versions
- **No linker issues**: Everything compiles together
- **Portable**: Works everywhere

**Dependencies**:
- `miniaudio.h` - Audio engine
- `clay.h` - UI layout
- `sokol_*.h` - Graphics abstraction

---

## Decision 10: Two Versions, Raylib Recommended

### Context
Which version should users start with?

### Decision: **Recommend Raylib version for beginners, Sokol for production**

### Rationale

**Raylib (`main_raylib.c`)**:
- ✅ Complete working UI
- ✅ Easy to understand
- ✅ Fast iteration
- ✅ Perfect for learning
- ✅ Good enough for most projects

**Sokol (`main.c`)**:
- ⚠️ Needs rendering implementation
- ✅ Better architecture
- ✅ More scalable
- ✅ Maximum performance
- ✅ Production-ready foundation

---

## Future Decisions Needed

### Not Yet Decided

1. **Audio file format support**
   - Options: WAV only, WAV+MP3, WAV+MP3+FLAC
   - Consider: `dr_wav.h`, `dr_mp3.h`, `dr_flac.h`

2. **Plugin format**
   - Options: VST3, CLAP, LV2, custom
   - Consider: Licensing, cross-platform support

3. **Project file format**
   - Options: JSON, XML, Binary, Custom
   - Consider: Human-readable vs compact

4. **MIDI implementation**
   - Options: RtMidi, PortMidi, platform-specific
   - Consider: Timing precision requirements

5. **Undo/Redo system**
   - Options: Command pattern, state snapshots
   - Consider: Memory usage vs granularity

---

## Key Principles

Throughout all decisions, we followed these principles:

1. **Simplicity First**: Choose simple over clever
2. **Real-Time Safe**: Audio thread is sacred
3. **Explicit Control**: No hidden behavior
4. **Beginner Friendly**: Easy to understand and modify
5. **Scalable**: Can grow to professional level
6. **Educational**: Code teaches good practices
7. **Pragmatic**: Ship working code, iterate later

---

## Lessons Learned

### What Worked Well
- ✅ Separating layout (Clay) from rendering (Sokol/Raylib)
- ✅ Lock-free audio/UI communication
- ✅ Providing two implementation options
- ✅ Extensive documentation
- ✅ Header-only dependencies

### What Could Be Improved
- ⚠️ Sokol rendering bridge needs completion
- ⚠️ Could add more example effects
- ⚠️ Timeline view not implemented yet

---

## References

- [miniaudio](https://miniaud.io/)
- [Clay UI](https://github.com/nicbarker/clay)
- [Sokol](https://github.com/floooh/sokol)
- [Raylib](https://www.raylib.com/)
- [Real-Time Audio Programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing)

---

**Last Updated**: 2024  
**Status**: Active Development  
**Next Review**: After first production use

---

## How to Use This Document

- **For contributors**: Understand why things are the way they are
- **For learners**: See reasoning behind architectural choices
- **For future maintainers**: Context for potential refactoring
- **For decision-makers**: Template for new decisions

When making new decisions, add them to this document with:
1. Context (why are we deciding?)
2. Options (what did we consider?)
3. Decision (what did we choose?)
4. Rationale (why this choice?)
5. Trade-offs (what do we gain/lose?)