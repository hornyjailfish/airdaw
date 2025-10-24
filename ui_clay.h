// ui_clay.h - Clay UI Components for AirDAW
// Declarative UI layout with Clay, rendered with Raylib
#pragma once
#ifndef UI_CLAY_H
#define UI_CLAY_H

#include "vendor/clay/clay.h"
#include "audio_engine.h"
#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// UI STATE
// ============================================================================

typedef struct {
    // Clay memory arena
    Clay_Arena clay_arena;
    void* clay_memory;

    // Font
    Font* font;
    uint32_t font_count;

    // Window dimensions
    int window_width;
    int window_height;

   // Interaction state
    uint32_t active_slider_id;
    float slider_drag_start_value;
    Vector2 slider_drag_start_pos;

    // Mouse state
    Vector2 mouse_pos;
    bool mouse_pressed;
    bool mouse_down;
    bool mouse_released;

    // UI actions (set by UI, read by main)
    bool add_track_requested;
    int track_play_toggle;      // -1 = none, >= 0 = track index
    int track_mute_toggle;      // -1 = none, >= 0 = track index
    int track_solo_toggle;      // -1 = none, >= 0 = track index
    bool master_play_toggle;

    // Effect actions
    int track_add_effect;       // -1 = none, >= 0 = track index
    EffectType effect_to_add;
} UIState;


// Build the UI layout (call in main loop)
Clay_RenderCommandArray ui_build_layout(UIState* ui_state, AudioEngine* engine);

// Handle UI interactions and update audio engine
void ui_handle_interactions(UIState* ui_state, AudioEngine* engine);


#endif // UI_CLAY_H
