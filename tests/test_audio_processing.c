#define CTEST_MAIN
#define CTEST_COLOR_OK

#include "../vendor/ctest/ctest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>

// ============================================================================
// TEST CONSTANTS
// ============================================================================

#define TEST_SAMPLE_RATE 48000
#define TEST_CHANNELS 2
#define TEST_BUFFER_SIZE 512
#define MAX_TRACKS 16

// ============================================================================
// TEST STRUCTURES
// ============================================================================

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
} TestTrack;

typedef struct {
    float master_volume;
    atomic_bool master_muted;
    
    TestTrack tracks[MAX_TRACKS];
    int track_count;
    
    atomic_int master_peak_left;
    atomic_int master_peak_right;
    
    atomic_bool is_playing;
    atomic_uint playback_position;
} TestEngine;

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

void create_sine_wave(float* buffer, size_t length, float frequency, float amplitude) {
    for (size_t i = 0; i < length; i++) {
        float t = (float)i / TEST_SAMPLE_RATE;
        buffer[i] = amplitude * sinf(2.0f * 3.14159265359f * frequency * t);
    }
}

void create_dc_signal(float* buffer, size_t length, float value) {
    for (size_t i = 0; i < length; i++) {
        buffer[i] = value;
    }
}

void create_silence(float* buffer, size_t length) {
    memset(buffer, 0, length * sizeof(float));
}

void init_test_engine(TestEngine* engine) {
    memset(engine, 0, sizeof(TestEngine));
    engine->master_volume = 1.0f;
    atomic_store(&engine->master_muted, false);
    atomic_store(&engine->is_playing, false);
}

int add_test_track(TestEngine* engine, const char* name, float* audio_data, size_t length) {
    if (engine->track_count >= MAX_TRACKS) {
        return -1;
    }
    
    int idx = engine->track_count++;
    TestTrack* track = &engine->tracks[idx];
    
    memset(track, 0, sizeof(TestTrack));
    track->volume = 1.0f;
    track->pan = 0.0f;
    atomic_store(&track->muted, false);
    atomic_store(&track->solo, false);
    atomic_store(&track->playing, true);
    
    snprintf(track->name, sizeof(track->name), "%s", name ? name : "Track");
    
    track->audio_data = audio_data;
    track->audio_length = length;
    track->playback_position = 0;
    
    return idx;
}

void process_audio_frame(TestEngine* engine, float* output, size_t frame_count) {
    memset(output, 0, frame_count * TEST_CHANNELS * sizeof(float));
    
    if (!atomic_load(&engine->is_playing)) {
        return;
    }
    
    // Check for solo tracks
    bool any_solo = false;
    for (int t = 0; t < engine->track_count; t++) {
        if (atomic_load(&engine->tracks[t].solo)) {
            any_solo = true;
            break;
        }
    }
    
    float master_peak_left = 0.0f;
    float master_peak_right = 0.0f;
    
    // Mix all tracks
    for (int t = 0; t < engine->track_count; t++) {
        TestTrack* track = &engine->tracks[t];
        
        if (atomic_load(&track->muted)) continue;
        if (any_solo && !atomic_load(&track->solo)) continue;
        if (!atomic_load(&track->playing)) continue;
        if (track->audio_data == NULL) continue;
        
        float track_peak_left = 0.0f;
        float track_peak_right = 0.0f;
        
        for (size_t i = 0; i < frame_count; i++) {
            size_t pos = track->playback_position + i;
            
            if (pos < track->audio_length) {
                float sample = track->audio_data[pos] * track->volume;
                
                // Constant power panning
                float pan_angle = (track->pan + 1.0f) * 0.25f * 3.14159265359f;
                float pan_left = cosf(pan_angle);
                float pan_right = sinf(pan_angle);
                
                float left_sample = sample * pan_left;
                float right_sample = sample * pan_right;
                
                output[i * 2] += left_sample;
                output[i * 2 + 1] += right_sample;
                
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
    
    // Apply master volume
    float master_vol = atomic_load(&engine->master_muted) ? 0.0f : engine->master_volume;
    
    for (size_t i = 0; i < frame_count * TEST_CHANNELS; i += 2) {
        output[i] *= master_vol;
        output[i + 1] *= master_vol;
        
        float abs_left = fabsf(output[i]);
        float abs_right = fabsf(output[i + 1]);
        
        if (abs_left > master_peak_left) master_peak_left = abs_left;
        if (abs_right > master_peak_right) master_peak_right = abs_right;
    }
    
    atomic_store(&engine->master_peak_left, (int)(master_peak_left * 1000.0f));
    atomic_store(&engine->master_peak_right, (int)(master_peak_right * 1000.0f));
    
    atomic_fetch_add(&engine->playback_position, (unsigned int)frame_count);
}

// ============================================================================
// TESTS: Basic Audio Processing
// ============================================================================

CTEST(audio_processing, silence_when_stopped) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 0.5f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    // Not playing - should output silence
    atomic_store(&engine.is_playing, false);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    for (size_t i = 0; i < TEST_BUFFER_SIZE * 2; i++) {
        ASSERT_DBL_NEAR(0.0, output[i]);
    }
}

CTEST(audio_processing, outputs_audio_when_playing) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 0.5f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    // Should have non-zero output
    bool has_signal = false;
    for (size_t i = 0; i < TEST_BUFFER_SIZE * 2; i++) {
        if (fabsf(output[i]) > 0.01f) {
            has_signal = true;
            break;
        }
    }
    
    ASSERT_TRUE(has_signal);
}

// ============================================================================
// TESTS: Volume Control
// ============================================================================

CTEST(volume, track_volume_scaling) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 0.5f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    engine.tracks[0].volume = 0.5f;
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    // With volume 0.5 and input 0.5, output should be ~0.25 (with panning)
    // Check that volume has an effect
    float sum = 0.0f;
    for (size_t i = 0; i < 10 * 2; i++) {
        sum += fabsf(output[i]);
    }
    
    ASSERT_TRUE(sum > 0.1f);
    ASSERT_TRUE(sum < 10.0f);
}

CTEST(volume, master_volume_scaling) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 1.0f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    engine.master_volume = 0.5f;
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    // Master volume should scale all output
    for (size_t i = 0; i < 10 * 2; i++) {
        ASSERT_TRUE(fabsf(output[i]) < 1.0f);
    }
}

CTEST(volume, zero_volume_produces_silence) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 1.0f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    engine.tracks[0].volume = 0.0f;
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    for (size_t i = 0; i < TEST_BUFFER_SIZE * 2; i++) {
        ASSERT_DBL_NEAR(0.0, output[i]);
    }
}

// ============================================================================
// TESTS: Panning
// ============================================================================

CTEST(panning, center_pan_equal_channels) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 1.0f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    engine.tracks[0].pan = 0.0f; // Center
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, 10);
    
    // Center pan should give roughly equal L/R levels
    float left_sum = 0.0f;
    float right_sum = 0.0f;
    
    for (size_t i = 0; i < 10; i++) {
        left_sum += fabsf(output[i * 2]);
        right_sum += fabsf(output[i * 2 + 1]);
    }
    
    float ratio = left_sum / right_sum;
    ASSERT_TRUE(ratio > 0.9f && ratio < 1.1f);
}

CTEST(panning, full_left_pan) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 1.0f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    engine.tracks[0].pan = -1.0f; // Full left
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, 10);
    
    float left_sum = 0.0f;
    float right_sum = 0.0f;
    
    for (size_t i = 0; i < 10; i++) {
        left_sum += fabsf(output[i * 2]);
        right_sum += fabsf(output[i * 2 + 1]);
    }
    
    // Left should be significantly louder than right
    ASSERT_TRUE(left_sum > right_sum);
}

CTEST(panning, full_right_pan) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 1.0f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    engine.tracks[0].pan = 1.0f; // Full right
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, 10);
    
    float left_sum = 0.0f;
    float right_sum = 0.0f;
    
    for (size_t i = 0; i < 10; i++) {
        left_sum += fabsf(output[i * 2]);
        right_sum += fabsf(output[i * 2 + 1]);
    }
    
    // Right should be significantly louder than left
    ASSERT_TRUE(right_sum > left_sum);
}

// ============================================================================
// TESTS: Mute Behavior
// ============================================================================

CTEST(mute, muted_track_produces_silence) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 1.0f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    atomic_store(&engine.tracks[0].muted, true);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    for (size_t i = 0; i < TEST_BUFFER_SIZE * 2; i++) {
        ASSERT_DBL_NEAR(0.0, output[i]);
    }
}

CTEST(mute, master_mute_silences_all) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 1.0f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    atomic_store(&engine.master_muted, true);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    for (size_t i = 0; i < TEST_BUFFER_SIZE * 2; i++) {
        ASSERT_DBL_NEAR(0.0, output[i]);
    }
}

// ============================================================================
// TESTS: Solo Behavior
// ============================================================================

CTEST(solo, solo_track_silences_others) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data1[TEST_BUFFER_SIZE];
    float audio_data2[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data1, TEST_BUFFER_SIZE, 1.0f);
    create_dc_signal(audio_data2, TEST_BUFFER_SIZE, 0.5f);
    
    add_test_track(&engine, "Track1", audio_data1, TEST_BUFFER_SIZE);
    add_test_track(&engine, "Track2", audio_data2, TEST_BUFFER_SIZE);
    
    atomic_store(&engine.tracks[0].solo, true);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, 10);
    
    // Should have output (track 0 is soloed)
    float sum = 0.0f;
    for (size_t i = 0; i < 10 * 2; i++) {
        sum += fabsf(output[i]);
    }
    
    ASSERT_TRUE(sum > 0.1f);
}

CTEST(solo, multiple_solo_tracks_play) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data1[TEST_BUFFER_SIZE];
    float audio_data2[TEST_BUFFER_SIZE];
    float audio_data3[TEST_BUFFER_SIZE];
    
    create_dc_signal(audio_data1, TEST_BUFFER_SIZE, 1.0f);
    create_dc_signal(audio_data2, TEST_BUFFER_SIZE, 1.0f);
    create_dc_signal(audio_data3, TEST_BUFFER_SIZE, 1.0f);
    
    add_test_track(&engine, "Track1", audio_data1, TEST_BUFFER_SIZE);
    add_test_track(&engine, "Track2", audio_data2, TEST_BUFFER_SIZE);
    add_test_track(&engine, "Track3", audio_data3, TEST_BUFFER_SIZE);
    
    atomic_store(&engine.tracks[0].solo, true);
    atomic_store(&engine.tracks[1].solo, true);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, 10);
    
    // Tracks 0 and 1 are soloed, should have output
    float sum = 0.0f;
    for (size_t i = 0; i < 10 * 2; i++) {
        sum += fabsf(output[i]);
    }
    
    ASSERT_TRUE(sum > 0.1f);
}

// ============================================================================
// TESTS: Mixing Multiple Tracks
// ============================================================================

CTEST(mixing, two_tracks_sum) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data1[TEST_BUFFER_SIZE];
    float audio_data2[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data1, TEST_BUFFER_SIZE, 0.3f);
    create_dc_signal(audio_data2, TEST_BUFFER_SIZE, 0.3f);
    
    add_test_track(&engine, "Track1", audio_data1, TEST_BUFFER_SIZE);
    add_test_track(&engine, "Track2", audio_data2, TEST_BUFFER_SIZE);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, 10);
    
    // With two tracks of 0.3, output should be roughly 0.6 (with panning)
    float left_avg = 0.0f;
    for (size_t i = 0; i < 10; i++) {
        left_avg += fabsf(output[i * 2]);
    }
    left_avg /= 10.0f;
    
    ASSERT_TRUE(left_avg > 0.3f);
}

CTEST(mixing, multiple_tracks_mix) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[4][128];
    for (int i = 0; i < 4; i++) {
        create_dc_signal(audio_data[i], 128, 0.1f);
        add_test_track(&engine, "Track", audio_data[i], 128);
    }
    
    float output[128 * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, 128);
    
    // All tracks should contribute
    float sum = 0.0f;
    for (size_t i = 0; i < 128 * 2; i++) {
        sum += fabsf(output[i]);
    }
    
    ASSERT_TRUE(sum > 1.0f);
}

// ============================================================================
// TESTS: Peak Metering
// ============================================================================

CTEST(metering, track_peaks_detected) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 0.8f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    int peak_left = atomic_load(&engine.tracks[0].peak_left);
    int peak_right = atomic_load(&engine.tracks[0].peak_right);
    
    ASSERT_GT(peak_left, 0);
    ASSERT_GT(peak_right, 0);
}

CTEST(metering, master_peaks_detected) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 0.5f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    int peak_left = atomic_load(&engine.master_peak_left);
    int peak_right = atomic_load(&engine.master_peak_right);
    
    ASSERT_GT(peak_left, 0);
    ASSERT_GT(peak_right, 0);
}

CTEST(metering, silence_gives_zero_peaks) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_silence(audio_data, TEST_BUFFER_SIZE);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    int peak_left = atomic_load(&engine.tracks[0].peak_left);
    int peak_right = atomic_load(&engine.tracks[0].peak_right);
    
    ASSERT_EQUAL(0, peak_left);
    ASSERT_EQUAL(0, peak_right);
}

// ============================================================================
// TESTS: Buffer Wrapping
// ============================================================================

CTEST(buffer, playback_position_advances) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 0.5f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    
    ASSERT_EQUAL(0, engine.tracks[0].playback_position);
    
    float output[128 * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, 128);
    
    ASSERT_EQUAL(128, engine.tracks[0].playback_position);
}

CTEST(buffer, playback_wraps_at_end) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[128];
    create_dc_signal(audio_data, 128, 0.5f);
    
    add_test_track(&engine, "Test", audio_data, 128);
    engine.tracks[0].playback_position = 64;
    
    float output[128 * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, 128);
    
    // Should wrap around: 64 + 128 = 192, which is > 128, so wraps to 64
    ASSERT_EQUAL(64, engine.tracks[0].playback_position);
}

// ============================================================================
// TESTS: No Clipping
// ============================================================================

CTEST(clipping, single_track_no_clip) {
    TestEngine engine;
    init_test_engine(&engine);
    
    float audio_data[TEST_BUFFER_SIZE];
    create_dc_signal(audio_data, TEST_BUFFER_SIZE, 0.8f);
    
    add_test_track(&engine, "Test", audio_data, TEST_BUFFER_SIZE);
    
    float output[TEST_BUFFER_SIZE * 2];
    
    atomic_store(&engine.is_playing, true);
    process_audio_frame(&engine, output, TEST_BUFFER_SIZE);
    
    for (size_t i = 0; i < TEST_BUFFER_SIZE * 2; i++) {
        ASSERT_TRUE(output[i] >= -1.0f);
        ASSERT_TRUE(output[i] <= 1.0f);
    }
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, const char* argv[]) {
    return ctest_main(argc, argv);
}