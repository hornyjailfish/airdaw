#define MINIAUDIO_IMPLEMENTATION
#include "vendor/miniaudio/miniaudio.h"

#include "audio_engine.h"
#include <raylib.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================================
// LOGGING
// ============================================================================

static void miniaudio_log_callback(void* pUserData, unsigned int level, const char* pMessage) {
    (void)pUserData;

    int raylib_level = LOG_INFO;

    switch (level) {
        case MA_LOG_LEVEL_DEBUG:
            raylib_level = LOG_DEBUG;
            break;
        case MA_LOG_LEVEL_INFO:
            raylib_level = LOG_INFO;
            break;
        case MA_LOG_LEVEL_WARNING:
            raylib_level = LOG_WARNING;
            break;
        case MA_LOG_LEVEL_ERROR:
            raylib_level = LOG_ERROR;
            break;
        default:
            raylib_level = LOG_INFO;

    }
    TraceLog(raylib_level, "[miniaudio]: %s", pMessage);
}

// ============================================================================
// EFFECT PROCESSING
// ============================================================================

static float process_gain_effect(float sample, Effect* effect) {
    if (!effect->enabled) { return sample; }
    return sample * effect->gain_params.gain;
}

static float process_lowpass_effect(float sample, Effect* effect, float* state) {
    if (!effect->enabled) { return sample; }

    // Simple one-pole lowpass filter
    // alpha = cutoff / (cutoff + 1)
    float cutoff = effect->filter_params.cutoff;
    float alpha = cutoff / (cutoff + 1.0f);
    *state = alpha * sample + (1.0f - alpha) * (*state);
    return *state;
}

static float process_highpass_effect(float sample, Effect* effect, float* state) {
    if (!effect->enabled) return sample;

    // Simple one-pole highpass filter
    float cutoff = effect->filter_params.cutoff;
    float alpha = 1.0f / (cutoff + 1.0f);
    float output = sample - *state;
    *state = *state + alpha * output;
    return output;
}

static void process_track_effects(Track* track, int track_idx, float* left, float* right, ma_uint32 frame_count) {
    // Static filter state per track (survives across callbacks)
    static float filter_state_l[MAX_TRACKS] = {0};
    static float filter_state_r[MAX_TRACKS] = {0};

    for (int i = 0; i < track->effect_count; i++) {
        Effect* effect = &track->effects[i];

        switch (effect->type) {
            case EFFECT_GAIN:
                for (ma_uint32 f = 0; f < frame_count; f++) {
                    left[f] = process_gain_effect(left[f], effect);
                    right[f] = process_gain_effect(right[f], effect);
                }
                break;

            case EFFECT_LOWPASS:
                for (ma_uint32 f = 0; f < frame_count; f++) {
                    left[f] = process_lowpass_effect(left[f], effect, &filter_state_l[track_idx]);
                    right[f] = process_lowpass_effect(right[f], effect, &filter_state_r[track_idx]);
                }
                break;

            case EFFECT_HIGHPASS:
                for (ma_uint32 f = 0; f < frame_count; f++) {
                    left[f] = process_highpass_effect(left[f], effect, &filter_state_l[track_idx]);
                    right[f] = process_highpass_effect(right[f], effect, &filter_state_r[track_idx]);
                }
                break;

            case EFFECT_DELAY:
                // TODO: Implement delay effect with circular buffer
                break;

            case EFFECT_REVERB:
                // TODO: Implement reverb effect
                break;

            default:
                break;
        }
    }
}

// ============================================================================
// AUDIO CALLBACK (REAL-TIME AUDIO THREAD)
// ============================================================================

static void audio_callback(ma_device* device, void* output_buffer, const void* input_buffer, ma_uint32 frame_count) {
    (void)input_buffer;

    AudioEngine* engine = (AudioEngine*)device->pUserData;
    float* out = (float*)output_buffer;

    if (!atomic_load(&engine->playing)) {
        memset(out, 0, frame_count * CHANNELS * sizeof(float));
        return;
    }

    // Clear output buffer
    memset(out, 0, frame_count * CHANNELS * sizeof(float));

    // Reset master meters
    engine->master_peak[0] = 0.0F;
    engine->master_peak[1] = 0.0F;
    engine->master_rms[0] = 0.0F;
    engine->master_rms[1] = 0.0F;

    // Check if any tracks are soloed
    bool any_solo = false;
    for (int t = 0; t < engine->track_count; t++) {
        if (engine->tracks[t].solo) {
            any_solo = true;
            break;
        }
    }

    // Mix all tracks
    for (int t = 0; t < engine->track_count; t++) {
        Track* track = &engine->tracks[t];

        if (track->mute || !atomic_load(&track->playing)) continue;

        if (any_solo && !track->solo) continue;

        // Reset track meters
        track->peak_level[0] = 0.0F;
        track->peak_level[1] = 0.0F;
        track->rms_level[0] = 0.0F;
        track->rms_level[1] = 0.0F;

        // Temporary buffers for effect processing
        float temp_left[BUFFER_SIZE];
        float temp_right[BUFFER_SIZE];

        // Generate audio (simple sine wave oscillator)
        for (ma_uint32 i = 0; i < frame_count; i++) {
            float sample = sinf(track->phase) * track->volume * 0.3F;
            track->phase += 2.0F * MA_PI * track->frequency / SAMPLE_RATE;
            if (track->phase > 2.0F * MA_PI) {
                track->phase -= 2.0F * MA_PI;
            }

            // Apply panning (constant power)
            float pan = track->pan;
            float left_gain = cosf((pan + 1.0F) * MA_PI / 4.0F);
            float right_gain = sinf((pan + 1.0F) * MA_PI / 4.0F);
ma_panner panyi;
            temp_left[i] = sample * left_gain;
            temp_right[i] = sample * right_gain;
        }

        // Process effects chain
        if (track->effect_count > 0) {
            process_track_effects(track, t, temp_left, temp_right, frame_count);
        }

        // Mix into output and compute meters
        for (ma_uint32 i = 0; i < frame_count; i++) {
            out[i * 2 + 0] += temp_left[i];
            out[i * 2 + 1] += temp_right[i];

            // Track metering
            float abs_left = fabsf(temp_left[i]);
            float abs_right = fabsf(temp_right[i]);
            if (abs_left > track->peak_level[0]) track->peak_level[0] = abs_left;
            if (abs_right > track->peak_level[1]) track->peak_level[1] = abs_right;
            track->rms_level[0] += temp_left[i] * temp_left[i];
            track->rms_level[1] += temp_right[i] * temp_right[i];
        }

        // Finalize RMS calculation
        track->rms_level[0] = sqrtf(track->rms_level[0] / frame_count);
        track->rms_level[1] = sqrtf(track->rms_level[1] / frame_count);
    }

    // Apply master volume and compute master meters
    for (ma_uint32 i = 0; i < frame_count; i++) {
        out[i * 2 + 0] *= engine->master_volume;
        out[i * 2 + 1] *= engine->master_volume;

        float abs_left = fabsf(out[i * 2 + 0]);
        float abs_right = fabsf(out[i * 2 + 1]);

        if (abs_left > engine->master_peak[0]) engine->master_peak[0] = abs_left;
        if (abs_right > engine->master_peak[1]) engine->master_peak[1] = abs_right;

        engine->master_rms[0] += out[i * 2 + 0] * out[i * 2 + 0];
        engine->master_rms[1] += out[i * 2 + 1] * out[i * 2 + 1];
    }

    engine->master_rms[0] = sqrtf(engine->master_rms[0] / frame_count);
    engine->master_rms[1] = sqrtf(engine->master_rms[1] / frame_count);
}

// ============================================================================
// AUDIO ENGINE API
// ============================================================================

bool audio_engine_init(AudioEngine* engine) {
    /* memset(engine, 0, sizeof(AudioEngine)); */

    engine->master_volume = 0.75F;
    engine->track_count = 0;
    atomic_store(&engine->playing, false);

    // Initialize miniaudio logging
    ma_allocation_callbacks alloc_cb = ma_allocation_callbacks_init_default();
    ma_log_init(&alloc_cb, &engine->log);
    ma_log_register_callback(&engine->log,ma_log_callback_init(miniaudio_log_callback, NULL));

    // Configure miniaudio device
    engine->device_config = ma_device_config_init(ma_device_type_playback);
    engine->device_config.playback.format = ma_format_f32;
    engine->device_config.playback.channels = CHANNELS;
    engine->device_config.sampleRate = SAMPLE_RATE;
    engine->device_config.dataCallback = audio_callback;
    engine->device_config.pUserData = engine;
    engine->device_config.periodSizeInFrames = BUFFER_SIZE;

    // Initialize device
    if (ma_device_init(NULL, &engine->device_config, &engine->device) != MA_SUCCESS) {
        ma_log_post(&engine->log, MA_LOG_LEVEL_ERROR, "Failed to initialize audio device");
        ma_log_uninit(&engine->log);
        return false;
    }
    engine->device.pContext->pLog = &engine->log;
    ma_log *lg = ma_device_get_log(&engine->device);
    ma_log_post(lg, MA_LOG_LEVEL_INFO, "TEST LOGING");
    ma_log_postf(&engine->log, MA_LOG_LEVEL_INFO, "Audio device initialized: %s", engine->device.playback.name);
    ma_log_postf(&engine->log, MA_LOG_LEVEL_INFO, "Format: %s, Channels: %u, Sample Rate: %u",
             ma_get_format_name(engine->device.playback.format),
             engine->device.playback.channels,
             engine->device.sampleRate);

    // Start device
    if (ma_device_start(&engine->device) != MA_SUCCESS) {
        ma_log_post(&engine->log, MA_LOG_LEVEL_ERROR, "Failed to start audio device");
        ma_device_uninit(&engine->device);
        ma_log_uninit(&engine->log);
        return false;
    }

    atomic_store(&engine->initialized, true);
    ma_log_post(&engine->log, MA_LOG_LEVEL_INFO, "Audio engine started successfully");
    return true;
}

void audio_engine_shutdown(AudioEngine* engine) {
    if (atomic_load(&engine->initialized)) {
        TraceLog(LOG_INFO, "[miniaudio] Shutting down audio engine...");

        atomic_store(&engine->playing, false);
        ma_device_uninit(&engine->device);
        ma_log_uninit(&engine->log);
        atomic_store(&engine->initialized, false);

        TraceLog(LOG_INFO, "[miniaudio] Audio engine shut down");
    }
}

int audio_engine_add_track(AudioEngine* engine, const char* name, float frequency) {
    if (engine->track_count >= MAX_TRACKS) {
        TraceLog(LOG_WARNING, "[miniaudio] Cannot add track: maximum tracks reached (%d)", MAX_TRACKS);
        return -1;
    }

    int index = engine->track_count++;
    Track* track = &engine->tracks[index];

    snprintf(track->name, sizeof(track->name), "%s", name);
    track->volume = 0.75F;
    track->pan = 0.0F;
    track->mute = false;
    track->solo = false;
    track->armed = false;
    track->frequency = frequency;
    track->phase = 0.0F;
    track->effect_count = 0;
    atomic_store(&track->playing, false);

    TraceLog(LOG_INFO, "[miniaudio] Added track %d: %s (%.1f Hz)", index, name, frequency);
    return index;
}

void audio_engine_add_effect(Track* track, EffectType type) {
    if (track->effect_count >= MAX_EFFECTS_PER_TRACK) {
        TraceLog(LOG_WARNING, "[miniaudio] Cannot add effect: maximum effects reached (%d)", MAX_EFFECTS_PER_TRACK);
        return;
    }

    Effect* effect = &track->effects[track->effect_count++];
    effect->type = type;
    effect->enabled = true;

    // Set default parameters
    switch (type) {
        case EFFECT_GAIN:
            effect->gain_params.gain = 1.0f;
            break;
        case EFFECT_LOWPASS:
        case EFFECT_HIGHPASS:
            effect->filter_params.cutoff = 1000.0f;
            effect->filter_params.resonance = 1.0f;
            break;
        case EFFECT_DELAY:
            effect->delay_params.time_ms = 250.0f;
            effect->delay_params.feedback = 0.3f;
            effect->delay_params.mix = 0.5f;
            break;
        case EFFECT_REVERB:
            effect->reverb_params.room_size = 0.5f;
            effect->reverb_params.damping = 0.5f;
            effect->reverb_params.mix = 0.3f;
            break;
        default:
            break;
    }

    TraceLog(LOG_INFO, "[miniaudio] Added effect type %d to track '%s'", type, track->name);
}

void audio_engine_remove_effect(Track* track, int effect_index) {
    if (effect_index < 0 || effect_index >= track->effect_count) {
        TraceLog(LOG_WARNING, "[miniaudio] Invalid effect index: %d", effect_index);
        return;
    }

    // Shift effects down
    for (int i = effect_index; i < track->effect_count - 1; i++) {
        track->effects[i] = track->effects[i + 1];
    }
    track->effect_count--;

    TraceLog(LOG_INFO, "[miniaudio] Removed effect %d from track '%s'", effect_index, track->name);
}

void audio_engine_toggle_effect(Track* track, int effect_index) {
    if (effect_index < 0 || effect_index >= track->effect_count) {
        TraceLog(LOG_WARNING, "[miniaudio] Invalid effect index: %d", effect_index);
        return;
    }

    track->effects[effect_index].enabled = !track->effects[effect_index].enabled;
    TraceLog(LOG_DEBUG, "[miniaudio] Toggled effect %d on track '%s': %s",
             effect_index, track->name,
             track->effects[effect_index].enabled ? "ON" : "OFF");
}

void audio_engine_set_effect_param(Effect* effect, int param_index, float value) {
    switch (effect->type) {
        case EFFECT_GAIN:
            if (param_index == 0) effect->gain_params.gain = value;
            break;
        case EFFECT_LOWPASS:
        case EFFECT_HIGHPASS:
            if (param_index == 0) effect->filter_params.cutoff = value;
            else if (param_index == 1) effect->filter_params.resonance = value;
            break;
        case EFFECT_DELAY:
            if (param_index == 0) effect->delay_params.time_ms = value;
            else if (param_index == 1) effect->delay_params.feedback = value;
            else if (param_index == 2) effect->delay_params.mix = value;
            break;
        case EFFECT_REVERB:
            if (param_index == 0) effect->reverb_params.room_size = value;
            else if (param_index == 1) effect->reverb_params.damping = value;
            else if (param_index == 2) effect->reverb_params.mix = value;
            break;
        default:
            break;
    }
}
