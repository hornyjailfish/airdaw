#define CTEST_MAIN
#define CTEST_COLOR_OK

#include "../vendor/ctest/ctest.h"
#include "../vendor/miniaudio/miniaudio.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <math.h>
#include <time.h>

// ============================================================================
// INTEGRATION TEST STRUCTURES (Full System)
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
} IntegrationTrack;

typedef struct {
    float master_volume;
    atomic_bool master_muted;
    
    IntegrationTrack tracks[MAX_TRACKS];
    int track_count;
    
    atomic_int master_peak_left;
    atomic_int master_peak_right;
    
    ma_device audio_device;
    ma_device_config device_config;
    
    atomic_bool is_playing;
    atomic_uint playback_position;
    
    // Statistics for testing
    atomic_uint callback_count;
    atomic_uint total_samples_processed;
} IntegrationEngine;

// ============================================================================
// INTEGRATION ENGINE IMPLEMENTATION
// ============================================================================

void integration_audio_callback(ma_device* device, void* output_buffer, 
                                 const void* input_buffer, ma_uint32 frame_count) {
    (void)input_buffer;
    
    IntegrationEngine* engine = (IntegrationEngine*)device->pUserData;
    float* out = (float*)output_buffer;
    
    atomic_fetch_add(&engine->callback_count, 1);
    atomic_fetch_add(&engine->total_samples_processed, frame_count);
    
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
        IntegrationTrack* track = &engine->tracks[t];
        
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
                
                float pan_angle = (track->pan + 1.0f) * 0.25f * 3.14159265359f;
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

bool integration_engine_init(IntegrationEngine* engine) {
    memset(engine, 0, sizeof(IntegrationEngine));
    
    engine->master_volume = 0.8f;
    atomic_store(&engine->master_muted, false);
    atomic_store(&engine->is_playing, false);
    atomic_store(&engine->callback_count, 0);
    atomic_store(&engine->total_samples_processed, 0);
    
    engine->device_config = ma_device_config_init(ma_device_type_playback);
    engine->device_config.playback.format = ma_format_f32;
    engine->device_config.playback.channels = CHANNELS;
    engine->device_config.sampleRate = SAMPLE_RATE;
    engine->device_config.dataCallback = integration_audio_callback;
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

void integration_engine_shutdown(IntegrationEngine* engine) {
    ma_device_uninit(&engine->audio_device);
    
    for (int i = 0; i < engine->track_count; i++) {
        if (engine->tracks[i].audio_data) {
            free(engine->tracks[i].audio_data);
        }
    }
}

int integration_add_track(IntegrationEngine* engine, const char* name, 
                          float frequency, float amplitude) {
    if (engine->track_count >= MAX_TRACKS) {
        return -1;
    }
    
    int idx = engine->track_count++;
    IntegrationTrack* track = &engine->tracks[idx];
    
    memset(track, 0, sizeof(IntegrationTrack));
    track->volume = 0.7f;
    track->pan = 0.0f;
    atomic_store(&track->muted, false);
    atomic_store(&track->solo, false);
    atomic_store(&track->playing, false);
    
    snprintf(track->name, sizeof(track->name), "%s", name ? name : "Track");
    
    track->audio_length = SAMPLE_RATE * 1;
    track->audio_data = (float*)malloc(track->audio_length * sizeof(float));
    
    for (size_t i = 0; i < track->audio_length; i++) {
        float t = (float)i / SAMPLE_RATE;
        track->audio_data[i] = amplitude * sinf(2.0f * 3.14159265359f * frequency * t);
    }
    
    return idx;
}

void sleep_ms(int milliseconds) {
#ifdef _WIN32
    Sleep(milliseconds);
#else
    struct timespec ts;
    ts.tv_sec = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

// ============================================================================
// INTEGRATION TESTS: Full System
// ============================================================================

CTEST(integration, full_init_shutdown_cycle) {
    IntegrationEngine engine;
    
    bool result = integration_engine_init(&engine);
    ASSERT_TRUE(result);
    
    sleep_ms(50);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, add_tracks_and_play) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    integration_add_track(&engine, "Track 1", 220.0f, 0.3f);
    integration_add_track(&engine, "Track 2", 330.0f, 0.3f);
    integration_add_track(&engine, "Track 3", 440.0f, 0.3f);
    
    ASSERT_EQUAL(3, engine.track_count);
    
    atomic_store(&engine.is_playing, true);
    for (int i = 0; i < 3; i++) {
        atomic_store(&engine.tracks[i].playing, true);
    }
    
    sleep_ms(100);
    
    unsigned int callbacks = atomic_load(&engine.callback_count);
    ASSERT_GT_U(callbacks, 0);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, transport_control) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    integration_add_track(&engine, "Test", 440.0f, 0.5f);
    atomic_store(&engine.tracks[0].playing, true);
    
    // Start
    atomic_store(&engine.is_playing, true);
    sleep_ms(50);
    unsigned int callbacks_playing = atomic_load(&engine.callback_count);
    
    // Stop
    atomic_store(&engine.is_playing, false);
    sleep_ms(50);
    unsigned int callbacks_stopped = atomic_load(&engine.callback_count);
    
    // Start again
    atomic_store(&engine.is_playing, true);
    sleep_ms(50);
    unsigned int callbacks_resumed = atomic_load(&engine.callback_count);
    
    ASSERT_GT_U(callbacks_playing, 0);
    ASSERT_GT_U(callbacks_resumed, callbacks_stopped);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, peak_metering_updates) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    integration_add_track(&engine, "Test", 440.0f, 0.5f);
    
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    
    sleep_ms(100);
    
    int peak_left = atomic_load(&engine.tracks[0].peak_left);
    int peak_right = atomic_load(&engine.tracks[0].peak_right);
    int master_left = atomic_load(&engine.master_peak_left);
    int master_right = atomic_load(&engine.master_peak_right);
    
    ASSERT_GT(peak_left, 0);
    ASSERT_GT(peak_right, 0);
    ASSERT_GT(master_left, 0);
    ASSERT_GT(master_right, 0);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, mute_during_playback) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    integration_add_track(&engine, "Test", 440.0f, 0.5f);
    
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    
    sleep_ms(50);
    int peak_before = atomic_load(&engine.master_peak_left);
    
    atomic_store(&engine.tracks[0].muted, true);
    sleep_ms(50);
    int peak_muted = atomic_load(&engine.master_peak_left);
    
    atomic_store(&engine.tracks[0].muted, false);
    sleep_ms(50);
    int peak_unmuted = atomic_load(&engine.master_peak_left);
    
    ASSERT_GT(peak_before, 10);
    ASSERT_EQUAL(0, peak_muted);
    ASSERT_GT(peak_unmuted, 10);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, solo_isolation) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    integration_add_track(&engine, "Track 1", 220.0f, 0.5f);
    integration_add_track(&engine, "Track 2", 440.0f, 0.5f);
    integration_add_track(&engine, "Track 3", 660.0f, 0.5f);
    
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    atomic_store(&engine.tracks[1].playing, true);
    atomic_store(&engine.tracks[2].playing, true);
    
    sleep_ms(50);
    int master_all = atomic_load(&engine.master_peak_left);
    
    atomic_store(&engine.tracks[1].solo, true);
    sleep_ms(50);
    int master_solo = atomic_load(&engine.master_peak_left);
    
    ASSERT_GT(master_all, 0);
    ASSERT_GT(master_solo, 0);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, master_volume_control) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    integration_add_track(&engine, "Test", 440.0f, 0.5f);
    
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    
    engine.master_volume = 1.0f;
    sleep_ms(50);
    int peak_full = atomic_load(&engine.master_peak_left);
    
    engine.master_volume = 0.5f;
    sleep_ms(50);
    int peak_half = atomic_load(&engine.master_peak_left);
    
    engine.master_volume = 0.0f;
    sleep_ms(50);
    int peak_zero = atomic_load(&engine.master_peak_left);
    
    ASSERT_GT(peak_full, peak_half);
    ASSERT_EQUAL(0, peak_zero);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, multiple_tracks_mixing) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    for (int i = 0; i < 8; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Track %d", i + 1);
        integration_add_track(&engine, name, 220.0f + i * 55.0f, 0.2f);
        atomic_store(&engine.tracks[i].playing, true);
    }
    
    ASSERT_EQUAL(8, engine.track_count);
    
    atomic_store(&engine.is_playing, true);
    sleep_ms(100);
    
    unsigned int callbacks = atomic_load(&engine.callback_count);
    int master_peak = atomic_load(&engine.master_peak_left);
    
    ASSERT_GT_U(callbacks, 0);
    ASSERT_GT(master_peak, 0);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, rapid_state_changes) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    integration_add_track(&engine, "Test", 440.0f, 0.5f);
    atomic_store(&engine.tracks[0].playing, true);
    
    atomic_store(&engine.is_playing, true);
    
    for (int i = 0; i < 10; i++) {
        atomic_store(&engine.tracks[0].muted, true);
        sleep_ms(10);
        atomic_store(&engine.tracks[0].muted, false);
        sleep_ms(10);
    }
    
    unsigned int callbacks = atomic_load(&engine.callback_count);
    ASSERT_GT_U(callbacks, 0);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, playback_position_advances) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    integration_add_track(&engine, "Test", 440.0f, 0.5f);
    
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    
    unsigned int pos_start = atomic_load(&engine.playback_position);
    
    sleep_ms(100);
    
    unsigned int pos_end = atomic_load(&engine.playback_position);
    
    ASSERT_GT_U(pos_end, pos_start);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, max_tracks_stress_test) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    for (int i = 0; i < MAX_TRACKS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "Track %d", i + 1);
        int track_id = integration_add_track(&engine, name, 
                                            220.0f + i * 27.5f, 0.15f);
        ASSERT_EQUAL(i, track_id);
        atomic_store(&engine.tracks[i].playing, true);
    }
    
    ASSERT_EQUAL(MAX_TRACKS, engine.track_count);
    
    atomic_store(&engine.is_playing, true);
    sleep_ms(100);
    
    unsigned int callbacks = atomic_load(&engine.callback_count);
    ASSERT_GT_U(callbacks, 0);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, concurrent_ui_audio_thread_operations) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    integration_add_track(&engine, "Test", 440.0f, 0.5f);
    
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    
    for (int i = 0; i < 20; i++) {
        engine.tracks[0].volume = 0.5f + (i % 2) * 0.3f;
        engine.tracks[0].pan = -1.0f + (i % 3) * 0.5f;
        
        int peak = atomic_load(&engine.tracks[0].peak_left);
        (void)peak;
        
        sleep_ms(5);
    }
    
    unsigned int callbacks = atomic_load(&engine.callback_count);
    ASSERT_GT_U(callbacks, 0);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, audio_callback_performance) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    for (int i = 0; i < 4; i++) {
        integration_add_track(&engine, "Track", 220.0f + i * 110.0f, 0.3f);
        atomic_store(&engine.tracks[i].playing, true);
    }
    
    atomic_store(&engine.is_playing, true);
    
    sleep_ms(200);
    
    unsigned int callbacks = atomic_load(&engine.callback_count);
    unsigned int samples = atomic_load(&engine.total_samples_processed);
    
    ASSERT_GT_U(callbacks, 5);
    ASSERT_GT_U(samples, 1000);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, engine_restart) {
    IntegrationEngine engine;
    
    integration_engine_init(&engine);
    integration_add_track(&engine, "Test", 440.0f, 0.5f);
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    sleep_ms(50);
    integration_engine_shutdown(&engine);
    
    integration_engine_init(&engine);
    integration_add_track(&engine, "Test", 440.0f, 0.5f);
    atomic_store(&engine.is_playing, true);
    atomic_store(&engine.tracks[0].playing, true);
    sleep_ms(50);
    
    unsigned int callbacks = atomic_load(&engine.callback_count);
    ASSERT_GT_U(callbacks, 0);
    
    integration_engine_shutdown(&engine);
}

CTEST(integration, memory_cleanup) {
    IntegrationEngine engine;
    integration_engine_init(&engine);
    
    for (int i = 0; i < MAX_TRACKS; i++) {
        integration_add_track(&engine, "Track", 440.0f, 0.3f);
    }
    
    for (int i = 0; i < MAX_TRACKS; i++) {
        ASSERT_NOT_NULL(engine.tracks[i].audio_data);
    }
    
    integration_engine_shutdown(&engine);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, const char* argv[]) {
    printf("\n");
    printf("==========================================\n");
    printf("  AirDAW Integration Tests\n");
    printf("==========================================\n");
    printf("Testing full system with real audio device\n");
    printf("Sample Rate: %d Hz\n", SAMPLE_RATE);
    printf("Buffer Size: %d samples\n", BUFFER_SIZE);
    printf("Max Tracks: %d\n", MAX_TRACKS);
    printf("==========================================\n\n");
    
    return ctest_main(argc, argv);
}