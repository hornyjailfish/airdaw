# Multithreading Architecture for AirDAW

This document explains the multithreading design used in AirDAW and provides guidelines for maintaining thread safety in real-time audio applications.

## Table of Contents

1. [Why Multithreading Matters](#why-multithreading-matters)
2. [Thread Architecture](#thread-architecture)
3. [Thread Safety Techniques](#thread-safety-techniques)
4. [Real-Time Audio Rules](#real-time-audio-rules)
5. [Lock-Free Communication](#lock-free-communication)
6. [Implementation Details](#implementation-details)
7. [Common Pitfalls](#common-pitfalls)
8. [Best Practices](#best-practices)
9. [Performance Considerations](#performance-considerations)

---

## Why Multithreading Matters

### The Challenge

In a DAW, we have two conflicting requirements:

1. **Audio Processing**: Must be **real-time** and **deterministic**
   - Cannot tolerate delays or glitches
   - Runs at high priority on dedicated thread
   - Processing budget: ~10ms at 512 samples / 48kHz
   - **Cannot wait for anything**

2. **UI Rendering**: Needs to be **responsive** and **smooth**
   - Updates at 60fps (16.6ms per frame)
   - Handles user input, animations, meters
   - Lower priority than audio
   - **Can afford occasional frame drops**

**Problem**: If both run on the same thread, UI operations can block audio processing, causing dropouts and glitches.

**Solution**: Separate threads with lock-free communication.

---

## Thread Architecture

### Overview

```
┌─────────────────────────────────────────────────────────────┐
│                        MAIN THREAD                          │
│                    (Normal Priority)                        │
│                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐ │
│  │   UI Render  │  │  User Input  │  │  Visual Updates  │ │
│  │   60 fps     │  │  Mouse/Key   │  │  Meters/Waves    │ │
│  └──────────────┘  └──────────────┘  └──────────────────┘ │
│                                                             │
│           │                     │                           │
│           ▼                     ▼                           │
│  ┌───────────────────────────────────────────────────────┐ │
│  │        Shared State (atomic variables)                │ │
│  │  - volume, pan, mute, solo                           │ │
│  │  - playback position                                 │ │
│  │  - peak meters                                       │ │
│  └───────────────────────────────────────────────────────┘ │
│           ▲                     ▲                           │
│           │                     │                           │
└───────────┼─────────────────────┼───────────────────────────┘
            │                     │
┌───────────┼─────────────────────┼───────────────────────────┐
│           │                     │                           │
│  ┌────────▼──────────┐  ┌──────▼────────────────────────┐  │
│  │  Read State       │  │  Write State                  │  │
│  │  (volumes, etc)   │  │  (peak values, position)      │  │
│  └───────────────────┘  └───────────────────────────────┘  │
│                                                             │
│                    AUDIO THREAD                             │
│                  (High Priority)                            │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │          Audio Callback (real-time)                 │   │
│  │  - Mix all tracks                                   │   │
│  │  - Apply effects                                    │   │
│  │  - Output to sound card                             │   │
│  │  - NO locks, NO malloc, NO syscalls                 │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
└─────────────────────────────────────────────────────────────┘

      Optional: I/O Thread (Background)
┌─────────────────────────────────────────────────────────────┐
│  - Load audio files                                         │
│  - Save projects                                            │
│  - Resample audio                                           │
│  - Can use locks, malloc, file I/O                          │
└─────────────────────────────────────────────────────────────┘
```

### Thread Responsibilities

#### 1. Audio Thread (Highest Priority)
- **Function**: `audio_callback()` in miniaudio
- **Frequency**: Called by OS audio subsystem (~every 10ms with 512 buffer)
- **Priority**: Real-time (highest)
- **Purpose**: Generate audio samples
- **Rules**: 
  - ❌ NO memory allocation (malloc/free)
  - ❌ NO locks (mutex, semaphore)
  - ❌ NO system calls
  - ❌ NO file I/O
  - ❌ NO blocking operations
  - ✅ Only use atomics for synchronization
  - ✅ Keep processing deterministic and fast

#### 2. Main Thread (Normal Priority)
- **Function**: Main event loop
- **Frequency**: ~60 fps
- **Priority**: Normal
- **Purpose**: UI rendering, user interaction
- **Can**: 
  - ✅ Allocate memory
  - ✅ Use locks (but carefully)
  - ✅ Do I/O (but prefer I/O thread)
  - ✅ Take as long as needed (just drops frames)

#### 3. I/O Thread (Optional, Low Priority)
- **Function**: Background file operations
- **Frequency**: On-demand
- **Priority**: Low/background
- **Purpose**: Load/save without blocking audio or UI
- **Can**:
  - ✅ All blocking operations
  - ✅ Heavy computation
  - ✅ Network requests

---

## Thread Safety Techniques

### 1. Atomic Variables

**What**: Variables that can be read/written atomically (no partial updates)

**When**: Sharing simple state between threads

**How**: Use C11 `stdatomic.h`

```c
#include <stdatomic.h>

atomic_bool muted;
atomic_int peak_value;
atomic_uint playback_position;

// Set from UI thread
atomic_store(&muted, true);

// Read from audio thread
bool is_muted = atomic_load(&muted);
```

**Guarantees**:
- No torn reads/writes
- Memory ordering (sequential consistency by default)
- No data races

### 2. Lock-Free Ring Buffers

**What**: Circular buffer for passing data between threads

**When**: Streaming audio data, event queues

**Why**: No locks = no priority inversion

```c
// Single-producer, single-consumer ring buffer
typedef struct {
    float* buffer;
    atomic_uint write_pos;
    atomic_uint read_pos;
    size_t capacity;
} RingBuffer;

// Producer (any thread)
void ring_buffer_write(RingBuffer* rb, float value) {
    uint32_t write = atomic_load(&rb->write_pos);
    uint32_t read = atomic_load(&rb->read_pos);
    
    // Check if full
    if ((write + 1) % rb->capacity == read) {
        return; // Buffer full
    }
    
    rb->buffer[write] = value;
    atomic_store(&rb->write_pos, (write + 1) % rb->capacity);
}

// Consumer (audio thread)
bool ring_buffer_read(RingBuffer* rb, float* value) {
    uint32_t read = atomic_load(&rb->read_pos);
    uint32_t write = atomic_load(&rb->write_pos);
    
    if (read == write) {
        return false; // Buffer empty
    }
    
    *value = rb->buffer[read];
    atomic_store(&rb->read_pos, (read + 1) % rb->capacity);
    return true;
}
```

### 3. Message Passing

**What**: Send commands from UI to audio thread

**When**: Parameter changes, transport control

**How**: Lock-free queue + process in audio callback

```c
typedef enum {
    MSG_SET_VOLUME,
    MSG_SET_PAN,
    MSG_MUTE,
    MSG_SOLO,
} MessageType;

typedef struct {
    MessageType type;
    int track_id;
    float value;
} AudioMessage;

// UI thread: Send message
AudioMessage msg = {
    .type = MSG_SET_VOLUME,
    .track_id = 3,
    .value = 0.8f
};
message_queue_push(&g_msg_queue, msg);

// Audio thread: Process messages
void audio_callback(...) {
    AudioMessage msg;
    while (message_queue_pop(&g_msg_queue, &msg)) {
        switch (msg.type) {
            case MSG_SET_VOLUME:
                tracks[msg.track_id].volume = msg.value;
                break;
            // ...
        }
    }
    
    // Now do audio processing
}
```

### 4. Double Buffering

**What**: Two buffers - one for reading, one for writing

**When**: Large data structures, waveform displays

**How**: Swap pointers atomically

```c
typedef struct {
    float* buffer_a;
    float* buffer_b;
    atomic_bool use_a; // true = read A, write B
} DoubleBuffer;

// Writer thread (UI/I/O)
void update_waveform(DoubleBuffer* db, float* new_data, size_t len) {
    bool using_a = atomic_load(&db->use_a);
    float* write_buffer = using_a ? db->buffer_b : db->buffer_a;
    
    // Write to inactive buffer
    memcpy(write_buffer, new_data, len * sizeof(float));
    
    // Swap
    atomic_store(&db->use_a, !using_a);
}

// Reader thread (audio)
void read_waveform(DoubleBuffer* db, float* output, size_t len) {
    bool using_a = atomic_load(&db->use_a);
    float* read_buffer = using_a ? db->buffer_a : db->buffer_b;
    
    memcpy(output, read_buffer, len * sizeof(float));
}
```

---

## Real-Time Audio Rules

### The Golden Rules

#### ❌ NEVER in Audio Callback

1. **malloc() / free()** - Unpredictable timing, can take milliseconds
2. **new / delete** - Same as above (C++)
3. **Mutex locks** - Can wait indefinitely if UI thread holds lock
4. **File I/O** - Blocking system call
5. **printf() / logging** - Often uses locks internally
6. **System calls** - Unpredictable latency
7. **std::vector::push_back()** - May allocate
8. **Exceptions** - Allocation + unpredictable path (C++)
9. **Virtual functions (in hot path)** - Indirect calls can miss cache
10. **Complex branching** - Keep logic simple and predictable

#### ✅ ALWAYS Do

1. **Pre-allocate** - Allocate all buffers at initialization
2. **Use atomics** - For shared state
3. **Keep it simple** - Straight-line code is best
4. **Profile** - Measure actual timing
5. **Test** - Run under load, check for xruns (buffer underruns)

### Measuring Audio Thread Performance

```c
void audio_callback(ma_device* device, void* output, const void* input, 
                    ma_uint32 frame_count) {
    // Optional: Measure timing in development builds
    #ifdef DEBUG_TIMING
    uint64_t start = get_nanoseconds();
    #endif
    
    // Do audio processing...
    
    #ifdef DEBUG_TIMING
    uint64_t end = get_nanoseconds();
    uint64_t elapsed = end - start;
    
    // With 512 samples @ 48kHz, budget is ~10.67ms
    uint64_t budget_ns = (frame_count * 1000000000ULL) / SAMPLE_RATE;
    
    // Store for display (atomic)
    atomic_store(&g_debug.callback_time_ns, elapsed);
    atomic_store(&g_debug.callback_budget_ns, budget_ns);
    #endif
}
```

---

## Lock-Free Communication

### Why Lock-Free?

**Priority Inversion Problem**:
```
Time →
Audio Thread (high priority): [waiting for lock...........] [runs]
UI Thread (low priority):     [holds lock][...............]
                                    ↑
                              Audio thread blocked by
                              lower-priority thread!
```

**Solution**: Don't use locks between audio and UI threads.

### Current Implementation in AirDAW

```c
// In audio engine structures
typedef struct {
    float volume;           // Plain float - UI sets, audio reads
    atomic_bool muted;      // Atomic - both threads access
    atomic_bool solo;       // Atomic - both threads access
    float pan;              // Plain float - UI sets, audio reads
    
    atomic_bool playing;    // Atomic - transport control
    atomic_int peak_left;   // Atomic - audio writes, UI reads
    atomic_int peak_right;  // Atomic - audio writes, UI reads
} Track;
```

**Why some are atomic and some aren't?**

- **Atomics** (`muted`, `solo`, `playing`): 
  - Boolean flags that change behavior immediately
  - Must be consistent
  
- **Plain floats** (`volume`, `pan`): 
  - Gradual parameters
  - A partially-written value (rare) just causes brief glitch
  - Acceptable trade-off for better performance
  - Could use atomics if you want guaranteed consistency

### Relaxed vs Sequential Consistency

```c
// Default (sequential consistency) - safest
atomic_store(&flag, true);

// Relaxed ordering - faster but needs careful reasoning
atomic_store_explicit(&flag, true, memory_order_relaxed);
```

**When to use relaxed**:
- Peak meters (occasional stale value is fine)
- Non-critical state
- When you understand memory ordering deeply

**When to use sequential**:
- Transport control (play/stop)
- Mute/solo (must be immediate)
- When in doubt - it's safer

---

## Implementation Details

### AirDAW's Threading Model

```c
// 1. Audio Engine Initialization (Main Thread)
bool audio_engine_init(AudioEngine* engine) {
    // Pre-allocate all audio buffers
    for (int i = 0; i < MAX_TRACKS; i++) {
        engine->tracks[i].audio_data = malloc(MAX_SAMPLES * sizeof(float));
    }
    
    // Start audio device (creates audio thread internally)
    ma_device_start(&engine->audio_device);
}

// 2. Audio Callback (Audio Thread - called by miniaudio)
void audio_callback(ma_device* device, void* output, ...) {
    AudioEngine* engine = device->pUserData;
    
    // Read atomic state (set by UI thread)
    bool is_playing = atomic_load(&engine->is_playing);
    if (!is_playing) return;
    
    // Mix tracks
    for (int i = 0; i < engine->track_count; i++) {
        Track* track = &engine->tracks[i];
        
        if (atomic_load(&track->muted)) continue;
        
        // Process audio...
        float sample = track->audio_data[pos] * track->volume;
        
        // Update peak meters (write atomic state for UI)
        if (sample > peak) {
            atomic_store(&track->peak_left, (int)(peak * 1000.0f));
        }
    }
}

// 3. UI Thread (Main Thread)
void draw_track(Track* track) {
    // Read atomic state (set by audio thread)
    float peak = atomic_load(&track->peak_left) / 1000.0f;
    draw_meter(peak);
    
    // Handle user input
    if (button_clicked("MUTE")) {
        // Write atomic state (read by audio thread)
        bool current = atomic_load(&track->muted);
        atomic_store(&track->muted, !current);
    }
    
    // Update non-atomic state (audio reads without atomics)
    if (slider_changed) {
        track->volume = new_volume; // Plain write, audio thread reads
    }
}
```

---

## Common Pitfalls

### ❌ Pitfall 1: Hidden Allocations

```c
// BAD - printf allocates internally
void audio_callback(...) {
    printf("Processing frame\n"); // NEVER DO THIS
}

// BAD - Variable-length array on stack (can overflow)
void audio_callback(...) {
    float temp[frame_count]; // Dangerous if frame_count is large
}

// GOOD - Pre-allocated buffer
AudioEngine engine;
float temp_buffer[MAX_BUFFER_SIZE];

void audio_callback(...) {
    // Use pre-allocated buffer
}
```

### ❌ Pitfall 2: Forgetting Atomics

```c
// BAD - Data race!
// UI Thread
track->volume = 0.8f;

// Audio Thread  
float v = track->volume; // Might read partial value!

// GOOD - Use atomic or accept risk
atomic_store(&track->atomic_volume, 0.8f);
float v = atomic_load(&track->atomic_volume);
```

### ❌ Pitfall 3: Complex Logic in Callback

```c
// BAD - Too much branching, hard to predict timing
void audio_callback(...) {
    for each track {
        if (track->type == AUDIO) {
            if (track->has_plugin) {
                for each plugin {
                    if (plugin->enabled) {
                        // Process...
                    }
                }
            } else {
                // Process...
            }
        } else if (track->type == MIDI) {
            // Handle MIDI...
        }
    }
}

// GOOD - Flat, predictable structure
void audio_callback(...) {
    // Simple loop with minimal branching
    for (int i = 0; i < active_track_count; i++) {
        Track* track = active_tracks[i]; // Pre-filtered list
        process_track(track, output); // Simple function
    }
}
```

### ❌ Pitfall 4: Not Testing Under Load

```c
// Works fine with 4 tracks, explodes with 32 tracks!
// Always test with:
// - Maximum track count
// - Multiple plugins
// - CPU throttling (simulated old hardware)
// - Background load (other apps running)
```

---

## Best Practices

### 1. Initialization Pattern

```c
typedef struct {
    // Pre-allocated buffers
    float* mix_buffer;
    float* temp_buffer;
    size_t buffer_capacity;
    
    // Atomic state
    atomic_bool running;
    
    // Audio thread writes, UI thread reads
    atomic_int current_latency_us;
    
    // UI thread writes, audio thread reads
    atomic_bool transport_playing;
} AudioEngine;

bool audio_engine_init(AudioEngine* engine, size_t max_buffer_size) {
    // Allocate everything upfront
    engine->mix_buffer = calloc(max_buffer_size * 2, sizeof(float));
    engine->temp_buffer = calloc(max_buffer_size * 2, sizeof(float));
    engine->buffer_capacity = max_buffer_size;
    
    if (!engine->mix_buffer || !engine->temp_buffer) {
        return false; // Failed
    }
    
    // Initialize atomics
    atomic_init(&engine->running, true);
    atomic_init(&engine->transport_playing, false);
    
    // Start audio device
    return start_audio_device(engine);
}
```

### 2. Parameter Smoothing

```c
// Smooth parameter changes to avoid clicks
typedef struct {
    float target;
    float current;
    float smooth_time_samples;
} SmoothedParameter;

void smooth_parameter_update(SmoothedParameter* param, 
                             float new_target,
                             float sample_rate) {
    param->target = new_target;
    param->smooth_time_samples = 0.005f * sample_rate; // 5ms smooth
}

float smooth_parameter_next(SmoothedParameter* param) {
    if (fabsf(param->current - param->target) < 0.0001f) {
        param->current = param->target;
    } else {
        float alpha = 1.0f / param->smooth_time_samples;
        param->current += (param->target - param->current) * alpha;
    }
    return param->current;
}

// Usage in audio callback
void audio_callback(...) {
    float smooth_volume = smooth_parameter_next(&track->volume_param);
    sample *= smooth_volume;
}
```

### 3. Graceful Degradation

```c
void audio_callback(...) {
    uint64_t start = get_cycles();
    
    // Process audio...
    
    uint64_t elapsed = get_cycles() - start;
    uint64_t budget = get_budget_cycles(frame_count, sample_rate);
    
    // Track overruns
    if (elapsed > budget) {
        atomic_fetch_add(&engine->overrun_count, 1);
        
        // Optional: Reduce quality next time
        atomic_store(&engine->quality_level, QUALITY_LOW);
    } else if (elapsed < budget * 0.5f) {
        // We have headroom, can increase quality
        atomic_store(&engine->quality_level, QUALITY_HIGH);
    }
}
```

### 4. Debug Hooks (Development Only)

```c
#ifdef DEBUG_AUDIO_THREAD
    #define AUDIO_ASSERT(cond) if (!(cond)) { \
        atomic_store(&g_debug.assertion_failed, true); \
        return; \
    }
#else
    #define AUDIO_ASSERT(cond)
#endif

void audio_callback(...) {
    AUDIO_ASSERT(frame_count <= MAX_BUFFER_SIZE);
    AUDIO_ASSERT(output != NULL);
    
    // Process...
}
```

---

## Performance Considerations

### CPU Cache

```c
// BAD - Cache misses (scattered memory access)
for (int i = 0; i < frame_count; i++) {
    for (int t = 0; t < track_count; t++) {
        output[i] += tracks[t].audio[tracks[t].position + i];
    }
}

// GOOD - Better cache locality (sequential access)
for (int t = 0; t < track_count; t++) {
    Track* track = &tracks[t];
    for (int i = 0; i < frame_count; i++) {
        output[i] += track->audio[track->position + i];
    }
}
```

### SIMD Optimization (Advanced)

```c
// Use SIMD for bulk operations
#ifdef __SSE2__
#include <emmintrin.h>

void mix_tracks_simd(float* output, float** inputs, 
                     int num_tracks, int frame_count) {
    for (int i = 0; i < frame_count; i += 4) {
        __m128 sum = _mm_setzero_ps();
        
        for (int t = 0; t < num_tracks; t++) {
            __m128 input = _mm_load_ps(&inputs[t][i]);
            sum = _mm_add_ps(sum, input);
        }
        
        _mm_store_ps(&output[i], sum);
    }
}
#endif
```

### Measure, Don't Guess

```c
// Always profile actual performance
typedef struct {
    atomic_uint callback_count;
    atomic_uint total_time_us;
    atomic_uint max_time_us;
    atomic_uint overrun_count;
} AudioStats;

void print_audio_stats(AudioStats* stats) {
    uint32_t count = atomic_load(&stats->callback_count);
    uint32_t total = atomic_load(&stats->total_time_us);
    uint32_t max = atomic_load(&stats->max_time_us);
    uint32_t overruns = atomic_load(&stats->overrun_count);
    
    printf("Callbacks: %u\n", count);
    printf("Avg time: %.2f µs\n", (float)total / count);
    printf("Max time: %u µs\n", max);
    printf("Overruns: %u (%.2f%%)\n", overruns, 
           100.0f * overruns / count);
}
```

---

## Testing Checklist

- [ ] Run with maximum track count
- [ ] Test on slower hardware (throttle CPU)
- [ ] Run overnight (check for drift/leaks)
- [ ] Stress test with background load
- [ ] Profile with real-time profiler
- [ ] Check for xruns/dropouts
- [ ] Verify no allocations in callback
- [ ] Test thread sanitizer (TSan)
- [ ] Test with different buffer sizes
- [ ] Verify meter updates are smooth

---

## Further Reading

- [Real-Time Audio Programming 101](http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing)
- [Lock-Free Programming](https://preshing.com/20120612/an-introduction-to-lock-free-programming/)
- [Memory Ordering](https://en.cppreference.com/w/c/atomic/memory_order)
- [JUCE Audio Thread Rules](https://docs.juce.com/master/tutorial_processing_audio_input.html)
- [miniaudio Documentation](https://miniaud.io/docs/manual/index.html)

---

**Remember**: The audio thread is the most critical part of your DAW. Treat it with respect, keep it simple, and never compromise on real-time safety! 🎵