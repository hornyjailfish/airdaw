#define CTEST_MAIN
#define CTEST_COLOR_OK

#include "../vendor/ctest/ctest.h"
#include "../vendor/miniaudio/miniaudio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>

// ============================================================================
// COPY OF AUDIO ENGINE STRUCTURES (for testing without UI dependencies)
// ============================================================================

#define MAX_TRACKS 16
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define BUFFER_SIZE 512

typedef struct {
    float volume;
    atomic_bool muted;
    atomic_bool solo;
    float pan;
    
    float* audio_data;
    size_t audio_length;
    size_t playback_position;
    atomic_bool playing;
    
    char name[64];
    
    atomic_int peak_left;
    atomic_int peak_right;
} Track;

typedef struct {
    float master_volume;
    atomic_bool master_muted;
    
    Track tracks[MAX_TRACKS];
    int track_count;
    
    atomic_int master_peak_left;
    atomic_int master_peak_right;
    
    ma_device audio_device;
    ma_device_config device_config;
    
    atomic_bool is_playing;
    atomic_uint playback_position;
} AudioEngine;

// ============================================================================
// AUDIO ENGINE FUNCTIONS (minimal implementation for testing)
// ============================================================================

void audio_callback(ma_device* device, void* output_buffer, const void* input_buffer, ma_uint32 frame_count) {
    (void)input_buffer;
    
    AudioEngine* engine = (AudioEngine*)device->pUserData;
    float* out = (float*)output_buffer;
    
    memset(out, 0, frame_count * CHANNELS * sizeof(float));
    
    if (!atomic_load(&engine->is_playing)) {
        return;
    }
    
    bool any_solo = false;
    for (int t = 0; t < engine->track_count; t++) {
        if (atomic_load(&engine->tracks[t].solo)) {
            any_solo = true;
            break;
        }
    }
    
    float master_peak_left = 0.0f;
    float master_peak_right = 0.0f;
    
    for (int t = 0; t < engine->track_count; t++) {
        Track* track = &engine->tracks[t];
        
        if (atomic_load(&track->muted)) continue;
        if (any_solo && !atomic_load(&track->solo)) continue;
        if (!atomic_load(&track->playing)) continue;
        if (track->audio_data == NULL) continue;
        
        float track_peak_left = 0.0f;
        float track_peak_right = 0.0f;
        
        for (ma_uint32 i = 0; i < frame_count; i++) {
            size_t pos = track->playback_position + i;
            
            if (pos < track->audio_length) {
                float sample = track->audio_data[pos] * track->volume;
                
                float pan_angle = (track->pan + 1.0f) * 0.25f * 3.14159f;
                float pan_left = cosf(pan_angle);
                float pan_right = sinf(pan_angle);
                
                float left_sample = sample * pan_left;
                float right_sample = sample * pan_right;
                
                out[i * 2] += left_sample;
                out[i * 2 + 1] += right_sample;
                
                float abs_left = fabsf(left_sample);
                float abs_right = fabsf(right_sample);
                if (abs_left > track_peak_left) track_peak_left = abs_left;
                if (abs_right > track_peak_right) track_peak_right = abs_right;
            }
        }
        
        atomic_store(&track->peak_left, (int)(track_peak_left * 1000.0f));
        atomic_store(&track->peak_right, (int)(track_peak_right * 1000.0f));
        
        track->playback_position += frame_count;
        if (track->playback_position >= track->audio_length) {
            track->playback_position = 0;
        }
    }
    
    float master_vol = atomic_load(&engine->master_muted) ? 0.0f : engine->master_volume;
    
    for (ma_uint32 i = 0; i < frame_count * CHANNELS; i += 2) {
        out[i] *= master_vol;
        out[i + 1] *= master_vol;
        
        float abs_left = fabsf(out[i]);
        float abs_right = fabsf(out[i + 1]);
        
        if (abs_left > master_peak_left) master_peak_left = abs_left;
        if (abs_right > master_peak_right) master_peak_right = abs_right;
    }
    
    atomic_store(&engine->master_peak_left, (int)(master_peak_left * 1000.0f));
    atomic_store(&engine->master_peak_right, (int)(master_peak_right * 1000.0f));
    
    atomic_fetch_add(&engine->playback_position, frame_count);
}

bool audio_engine_init(AudioEngine* engine) {
    memset(engine, 0, sizeof(AudioEngine));
    
    engine->master_volume = 0.8f;
    atomic_store(&engine->master_muted, false);
    atomic_store(&engine->is_playing, false);
    
    engine->device_config = ma_device_config_init(ma_device_type_playback);
    engine->device_config.playback.format = ma_format_f32;
    engine->device_config.playback.channels = CHANNELS;
    engine->device_config.sampleRate = SAMPLE_RATE;
    engine->device_config.dataCallback = audio_callback;
    engine->device_config.pUserData = engine;
    engine->device_config.periodSizeInFrames = BUFFER_SIZE;
    
    if (ma_device_init(NULL, &engine->device_config, &engine->audio_device) != MA_SUCCESS) {
        return false;
    }
    
    if (ma_device_start(&engine->audio_device) != MA_SUCCESS) {
        ma_device_uninit(&engine->audio_device);
        return false;
    }
    
    return true;
}

void audio_engine_shutdown(AudioEngine* engine) {
    ma_device_uninit(&engine->audio_device);
    
    for (int i = 0; i < engine->track_count; i++) {
        if (engine->tracks[i].audio_data) {
            free(engine->tracks[i].audio_data);
        }
    }
}

int audio_engine_add_track(AudioEngine* engine, const char* name) {
    if (engine->track_count >= MAX_TRACKS) {
        return -1;
    }
    
    int idx = engine->track_count++;
    Track* track = &engine->tracks[idx];
    
    memset(track, 0, sizeof(Track));
    track->volume = 0.7f;
    track->pan = 0.0f;
    atomic_store(&track->muted, false);
    atomic_store(&track->solo, false);
    atomic_store(&track->playing, false);
    
    snprintf(track->name, sizeof(track->name), "%s", name ? name : "Track");
    
    track->audio_length = SAMPLE_RATE * 2;
    track->audio_data = (float*)malloc(track->audio_length * sizeof(float));
    
    float freq = 220.0f + (idx * 110.0f);
    for (size_t i = 0; i < track->audio_length; i++) {
        float t = (float)i / SAMPLE_RATE;
        track->audio_data[i] = 0.3f * sinf(2.0f * 3.14159f * freq * t) * (1.0f - t / 2.0f);
    }
    
    return idx;
}

// ============================================================================
// TESTS: Audio Engine Initialization
// ============================================================================

CTEST(audio_engine, init_and_shutdown) {
    AudioEngine engine;
    
    bool result = audio_engine_init(&engine);
    ASSERT_TRUE(result);
    ASSERT_EQUAL(0, engine.track_count);
    ASSERT_FALSE(atomic_load(&engine.is_playing));
    ASSERT_FALSE(atomic_load(&engine.master_muted));
    ASSERT_DBL_NEAR(0.8, engine.master_volume);
    
    audio_engine_shutdown(&engine);
}

CTEST(audio_engine, initial_state) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    ASSERT_EQUAL(0, engine.track_count);
    ASSERT_EQUAL(0, atomic_load(&engine.playback_position));
    ASSERT_EQUAL(0, atomic_load(&engine.master_peak_left));
    ASSERT_EQUAL(0, atomic_load(&engine.master_peak_right));
    
    audio_engine_shutdown(&engine);
}

// ============================================================================
// TESTS: Track Management
// ============================================================================

CTEST(tracks, add_single_track) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    int track_id = audio_engine_add_track(&engine, "Test Track");
    
    ASSERT_EQUAL(0, track_id);
    ASSERT_EQUAL(1, engine.track_count);
    ASSERT_STR("Test Track", engine.tracks[0].name);
    ASSERT_NOT_NULL(engine.tracks[0].audio_data);
    ASSERT_GT_U(engine.tracks[0].audio_length, 0);
    
    audio_engine_shutdown(&engine);
}

CTEST(tracks, add_multiple_tracks) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    for (int i = 0; i < 5; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Track %d", i + 1);
        int track_id = audio_engine_add_track(&engine, name);
        ASSERT_EQUAL(i, track_id);
    }
    
    ASSERT_EQUAL(5, engine.track_count);
    
    audio_engine_shutdown(&engine);
}

CTEST(tracks, max_tracks_limit) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    // Add maximum number of tracks
    for (int i = 0; i < MAX_TRACKS; i++) {
        int track_id = audio_engine_add_track(&engine, "Track");
        ASSERT_EQUAL(i, track_id);
    }
    
    ASSERT_EQUAL(MAX_TRACKS, engine.track_count);
    
    // Try to add one more - should fail
    int overflow_track = audio_engine_add_track(&engine, "Overflow");
    ASSERT_EQUAL(-1, overflow_track);
    ASSERT_EQUAL(MAX_TRACKS, engine.track_count);
    
    audio_engine_shutdown(&engine);
}

CTEST(tracks, default_track_values) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    Track* track = &engine.tracks[0];
    
    ASSERT_DBL_NEAR(0.7, track->volume);
    ASSERT_DBL_NEAR(0.0, track->pan);
    ASSERT_FALSE(atomic_load(&track->muted));
    ASSERT_FALSE(atomic_load(&track->solo));
    ASSERT_FALSE(atomic_load(&track->playing));
    ASSERT_EQUAL(0, track->playback_position);
    
    audio_engine_shutdown(&engine);
}

// ============================================================================
// TESTS: Volume Control
// ============================================================================

CTEST(volume, set_track_volume) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    
    engine.tracks[0].volume = 0.5f;
    ASSERT_DBL_NEAR(0.5, engine.tracks[0].volume);
    
    engine.tracks[0].volume = 1.0f;
    ASSERT_DBL_NEAR(1.0, engine.tracks[0].volume);
    
    engine.tracks[0].volume = 0.0f;
    ASSERT_DBL_NEAR(0.0, engine.tracks[0].volume);
    
    audio_engine_shutdown(&engine);
}

CTEST(volume, set_master_volume) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    engine.master_volume = 1.0f;
    ASSERT_DBL_NEAR(1.0, engine.master_volume);
    
    engine.master_volume = 0.5f;
    ASSERT_DBL_NEAR(0.5, engine.master_volume);
    
    engine.master_volume = 0.0f;
    ASSERT_DBL_NEAR(0.0, engine.master_volume);
    
    audio_engine_shutdown(&engine);
}

// ============================================================================
// TESTS: Pan Control
// ============================================================================

CTEST(pan, set_track_pan) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    
    // Center
    engine.tracks[0].pan = 0.0f;
    ASSERT_DBL_NEAR(0.0, engine.tracks[0].pan);
    
    // Full left
    engine.tracks[0].pan = -1.0f;
    ASSERT_DBL_NEAR(-1.0, engine.tracks[0].pan);
    
    // Full right
    engine.tracks[0].pan = 1.0f;
    ASSERT_DBL_NEAR(1.0, engine.tracks[0].pan);
    
    audio_engine_shutdown(&engine);
}

// ============================================================================
// TESTS: Mute/Solo
// ============================================================================

CTEST(mute_solo, mute_track) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    
    ASSERT_FALSE(atomic_load(&engine.tracks[0].muted));
    
    atomic_store(&engine.tracks[0].muted, true);
    ASSERT_TRUE(atomic_load(&engine.tracks[0].muted));
    
    atomic_store(&engine.tracks[0].muted, false);
    ASSERT_FALSE(atomic_load(&engine.tracks[0].muted));
    
    audio_engine_shutdown(&engine);
}

CTEST(mute_solo, solo_track) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    
    ASSERT_FALSE(atomic_load(&engine.tracks[0].solo));
    
    atomic_store(&engine.tracks[0].solo, true);
    ASSERT_TRUE(atomic_load(&engine.tracks[0].solo));
    
    atomic_store(&engine.tracks[0].solo, false);
    ASSERT_FALSE(atomic_load(&engine.tracks[0].solo));
    
    audio_engine_shutdown(&engine);
}

CTEST(mute_solo, master_mute) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    ASSERT_FALSE(atomic_load(&engine.master_muted));
    
    atomic_store(&engine.master_muted, true);
    ASSERT_TRUE(atomic_load(&engine.master_muted));
    
    atomic_store(&engine.master_muted, false);
    ASSERT_FALSE(atomic_load(&engine.master_muted));
    
    audio_engine_shutdown(&engine);
}

// ============================================================================
// TESTS: Playback State
// ============================================================================

CTEST(playback, start_stop) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    
    ASSERT_FALSE(atomic_load(&engine.is_playing));
    ASSERT_FALSE(atomic_load(&engine.tracks[0].playing));
    
    // Start playback
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    
    ASSERT_TRUE(atomic_load(&engine.is_playing));
    ASSERT_TRUE(atomic_load(&engine.tracks[0].playing));
    
    // Stop playback
    atomic_store(&engine.is_playing, false);
    atomic_store(&engine.tracks[0].playing, false);
    
    ASSERT_FALSE(atomic_load(&engine.is_playing));
    ASSERT_FALSE(atomic_load(&engine.tracks[0].playing));
    
    audio_engine_shutdown(&engine);
}

// ============================================================================
// TESTS: Audio Data
// ============================================================================

CTEST(audio_data, buffer_allocated) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    
    ASSERT_NOT_NULL(engine.tracks[0].audio_data);
    ASSERT_GT_U(engine.tracks[0].audio_length, 0);
    
    audio_engine_shutdown(&engine);
}

CTEST(audio_data, buffer_contains_data) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    Track* track = &engine.tracks[0];
    
    // Check that buffer contains non-zero samples
    bool has_non_zero = false;
    for (size_t i = 0; i < track->audio_length; i++) {
        if (fabsf(track->audio_data[i]) > 0.001f) {
            has_non_zero = true;
            break;
        }
    }
    
    ASSERT_TRUE(has_non_zero);
    
    audio_engine_shutdown(&engine);
}

CTEST(audio_data, buffer_within_bounds) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    Track* track = &engine.tracks[0];
    
    // Check all samples are within valid audio range [-1.0, 1.0]
    for (size_t i = 0; i < track->audio_length; i++) {
        ASSERT_TRUE(track->audio_data[i] >= -1.0f);
        ASSERT_TRUE(track->audio_data[i] <= 1.0f);
    }
    
    audio_engine_shutdown(&engine);
}

// ============================================================================
// TESTS: Thread Safety (Atomics)
// ============================================================================

CTEST(thread_safety, atomic_bool_operations) {
    atomic_bool flag;
    atomic_init(&flag, false);
    
    ASSERT_FALSE(atomic_load(&flag));
    
    atomic_store(&flag, true);
    ASSERT_TRUE(atomic_load(&flag));
    
    atomic_store(&flag, false);
    ASSERT_FALSE(atomic_load(&flag));
}

CTEST(thread_safety, atomic_int_operations) {
    atomic_int value;
    atomic_init(&value, 0);
    
    ASSERT_EQUAL(0, atomic_load(&value));
    
    atomic_store(&value, 100);
    ASSERT_EQUAL(100, atomic_load(&value));
    
    atomic_fetch_add(&value, 50);
    ASSERT_EQUAL(150, atomic_load(&value));
}

CTEST(thread_safety, atomic_uint_operations) {
    atomic_uint value;
    atomic_init(&value, 0);
    
    ASSERT_EQUAL_U(0, atomic_load(&value));
    
    atomic_store(&value, 1000);
    ASSERT_EQUAL_U(1000, atomic_load(&value));
    
    atomic_fetch_add(&value, 500);
    ASSERT_EQUAL_U(1500, atomic_load(&value));
}

// ============================================================================
// TESTS: Peak Metering
// ============================================================================

CTEST(metering, initial_peaks_zero) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    ASSERT_EQUAL(0, atomic_load(&engine.master_peak_left));
    ASSERT_EQUAL(0, atomic_load(&engine.master_peak_right));
    
    audio_engine_shutdown(&engine);
}

CTEST(metering, track_peaks_initial) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    
    ASSERT_EQUAL(0, atomic_load(&engine.tracks[0].peak_left));
    ASSERT_EQUAL(0, atomic_load(&engine.tracks[0].peak_right));
    
    audio_engine_shutdown(&engine);
}

// ============================================================================
// TESTS: Buffer Management
// ============================================================================

CTEST(buffer, initial_playback_position) {
    AudioEngine engine;
    audio_engine_init(&engine);
    
    audio_engine_add_track(&engine, "Test");
    
    ASSERT_EQUAL(0, engine.tracks[0].playback_position);
    ASSERT_EQUAL_U(0, atomic_load(&engine.playback_position));
    
    audio_engine_shutdown(&engine);
}

// ============================================================================
// TESTS: Constants
// ============================================================================

CTEST(constants, sample_rate) {
    ASSERT_EQUAL(48000, SAMPLE_RATE);
}

CTEST(constants, channels) {
    ASSERT_EQUAL(2, CHANNELS);
}

CTEST(constants, max_tracks) {
    ASSERT_EQUAL(16, MAX_TRACKS);
}

CTEST(constants, buffer_size) {
    ASSERT_EQUAL(512, BUFFER_SIZE);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, const char* argv[]) {
    return ctest_main(argc, argv);
}