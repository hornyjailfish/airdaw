// audio_engine.h - Audio Engine with Effects Support
// Handles real-time audio processing, track management, and per-channel effects
#pragma once
#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include "vendor/miniaudio/miniaudio.h"
#include <stdatomic.h>
#include <stdbool.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define MAX_TRACKS 16
#define SAMPLE_RATE 48000
#define CHANNELS 2
#define BUFFER_SIZE 512
#define MAX_EFFECTS_PER_TRACK 8

// ============================================================================
// EFFECT TYPES
// ============================================================================

typedef enum {
    EFFECT_NONE = 0,
    EFFECT_GAIN,
    EFFECT_LOWPASS,
    EFFECT_HIGHPASS,
    EFFECT_DELAY,
    EFFECT_REVERB
} EffectType;

typedef struct {
    EffectType type;
    bool enabled;

    union {
        struct {
            float gain;
        } gain_params;

        struct {
            float cutoff;
            float resonance;
        } filter_params;

        struct {
            float time_ms;
            float feedback;
            float mix;
        } delay_params;

        struct {
            float room_size;
            float damping;
            float mix;
        } reverb_params;
    };
} Effect;

// ============================================================================
// TRACK STRUCTURE
// ============================================================================

typedef struct {
    char name[64];

    // Mix controls
    float volume;           // 0.0 to 1.0
    float pan;              // -1.0 (left) to 1.0 (right)
    bool mute;
    bool solo;
    bool armed;

    // Audio generation (simple oscillator for now)
    float frequency;        // Oscillator frequency (Hz)
    float phase;            // Oscillator phase
    atomic_bool playing;

    // Effects chain
    Effect effects[MAX_EFFECTS_PER_TRACK];
    int effect_count;

    // Metering (updated by audio thread)
    float peak_level[2];    // Peak levels for L/R channels
    float rms_level[2];     // RMS levels for L/R channels
} Track;

// ============================================================================
// AUDIO ENGINE STRUCTURE
// ============================================================================

typedef struct {
    ma_device device;
    ma_device_config device_config;
    ma_log log;

    Track tracks[MAX_TRACKS];
    int track_count;

    float master_volume;
    float master_peak[2];
    float master_rms[2];

    atomic_bool playing;
    atomic_bool initialized;
} AudioEngine;

// ============================================================================
// AUDIO ENGINE API
// ============================================================================

// Initialize the audio engine
bool audio_engine_init(AudioEngine* engine);

// Shutdown the audio engine
void audio_engine_shutdown(AudioEngine* engine);

// Add a new track with given name and frequency
// Returns track index or -1 on failure
int audio_engine_add_track(AudioEngine* engine, const char* name, float frequency);

// Add an effect to a track's effect chain
void audio_engine_add_effect(Track* track, EffectType type);

// Remove an effect from a track's effect chain
void audio_engine_remove_effect(Track* track, int effect_index);

// Toggle effect enabled/disabled
void audio_engine_toggle_effect(Track* track, int effect_index);

// Set effect parameter (generic setter)
void audio_engine_set_effect_param(Effect* effect, int param_index, float value);

#endif // AUDIO_ENGINE_H
