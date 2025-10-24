#include <math.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_clay.h"
#include "vendor/clay/clay.h"

#include "audio_engine.h"
#include "raylib.h"
#include "renderer_utils.h"

// UI COMPONENTS
// ============================================================================
// ============================================================================
Clay_ElementDeclaration button(Clay_ElementId id, bool active, int *clicked_out,
                               UIState *ui_state) {
  /* Clay_ElementId button_id = CLAY_IDI("Button", id); */
  Clay_Color color = COLOR_BUTTON;
  if (Clay_PointerOver(id)) {
    color = COLOR_BUTTON_HOVER;
  }
  if (active) {
    color = COLOR_BUTTON_ACTIVE;
  }
  Clay_ElementDeclaration button = {
      .id = id,
      .backgroundColor = color,
      .layout = {.padding = {5, 5, 5, 5},
                 .sizing = {CLAY_SIZING_FIT(.min = 50), CLAY_SIZING_FIXED(25)}},
      .cornerRadius = CLAY_CORNER_RADIUS(3)};
  // Check for interaction
  if (clicked_out && Clay_PointerOver(id) && ui_state->mouse_pressed) {
    *clicked_out = 1;
  }
  return button;
};

void build_button(const char *label, uint32_t id, bool active, int *clicked_out,
                  UIState *ui_state) {
  Clay_String text = {
      .isStaticallyAllocated = false, .chars = label, .length = strlen(label)};
  Clay_ElementId uid = CLAY_SIDI(text,id);
  CLAY(button(uid, active, clicked_out, ui_state)) {
    CLAY_TEXT(text,
              CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT, .fontSize = 10}));
  };
};

static void build_vertical_fader(float value, uint32_t id, float height) {
  CLAY({
      .id = CLAY_IDI("Fader", id),
      .layout = {.sizing = {CLAY_SIZING_FIXED(30), CLAY_SIZING_FIXED(height)},
                 .childAlignment = {.y = CLAY_ALIGN_Y_BOTTOM}},
      .backgroundColor = COLOR_SLIDER_BG,
  }) {
    float fill_height = value * height;
    if (fill_height > 1.0F) {
      CLAY({.id = CLAY_IDI("FaderFill", id),
            .backgroundColor = COLOR_SLIDER,
            .layout = {.sizing = {CLAY_SIZING_FIXED(30),
                                  CLAY_SIZING_FIXED(fill_height)}}}) {}
    }
  }
}

void build_meter(float level, uint32_t id, float height, bool is_left) {
  Clay_Color meter_color = level > 0.9F   ? COLOR_METER_RED
                           : level > 0.7F ? COLOR_METER_YELLOW
                                          : COLOR_METER_GREEN;
  const char *meter_name_id = is_left ? "MeterL" : "MeterR";
  const char *meter_fill_id = is_left ? "MeterLFill" : "MeterRFill";
  CLAY({.id = CLAY_IDI("Meter", id),
        .layout = {.sizing = {CLAY_SIZING_FIXED(15), CLAY_SIZING_FIXED(height)},
                   .childAlignment = {.y = CLAY_ALIGN_Y_BOTTOM}},
        .backgroundColor = COLOR_SLIDER_BG}) {
    float fill_height = level * height;
    if (fill_height > 1.0F) {
      CLAY({.id = CLAY_IDI("MeterFill", id),
            .backgroundColor = meter_color,
            .layout = {.sizing = {CLAY_SIZING_FIXED(15),
                                  CLAY_SIZING_FIXED(fill_height)}}}) {}
    }
  }
}

void build_track_ui(UIState *ui_state, Track *track, int track_index) {
  int clicked = 0;
  Clay_String text = {.isStaticallyAllocated = false,
                      .chars = track->name,
                      .length = (int32_t)strlen(track->name)};
  CLAY({
      .id = CLAY_IDI("Track", track_index),
      .layout =
          {
              .sizing = {CLAY_SIZING_FIXED(180), CLAY_SIZING_FIXED(450)},
              .layoutDirection = CLAY_TOP_TO_BOTTOM,
              .padding = {10, 10, 10, 10},
              .childGap = 10,
          },
      .border = {.width = CLAY_BORDER_ALL(2), .color = COLOR_TRACK_BORDER},
      .cornerRadius = CLAY_CORNER_RADIUS(4),
      .backgroundColor = COLOR_TRACK_BG,
  }) {
    // Track name
    CLAY_TEXT(text,
              CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT, .fontSize = 12}));

    // Control buttons row
    CLAY({
        .id = CLAY_IDI("ButtonRow", track_index),
        .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .childGap = 5,
                   .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(25)}},
    }) {
      // Play button
      bool playing = atomic_load(&track->playing);
      clicked = 0;
      build_button(playing ? "STOP" : "PLAY", track_index, playing, &clicked,
                   ui_state);
      if (clicked) {
        ui_state->track_play_toggle = track_index;
      }

      // Mute button
      clicked = 0;
      build_button("M", track_index, track->mute, &clicked, ui_state);

      if (clicked) {
        ui_state->track_mute_toggle = track_index;
      }

      // Solo button
      clicked = 0;
      build_button("S", track_index, track->solo, &clicked, ui_state);
      if (clicked) {
        ui_state->track_solo_toggle = track_index;
      }
    }

    // Controls row (volume + meters)
    CLAY({.id = CLAY_IDI("ControlsRow", track_index),
          .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 10,
                     .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(250)}}}) {
      // Volume fader
      CLAY(
          {.id = CLAY_IDI("VolumeContainer", track_index),
           .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                      .childGap = 5,
                      .sizing = {CLAY_SIZING_FIXED(30), CLAY_SIZING_GROW()}}}) {
        build_vertical_fader(track->volume, track_index, 200);
        CLAY_TEXT(
            CLAY_STRING("VOL"),
            CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM, .fontSize = 8}));
      }

      // Pan control
      CLAY({.id = CLAY_IDI("PanContainer", track_index),
            .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                       .childGap = 5,
                       .sizing = {CLAY_SIZING_FIXED(60), CLAY_SIZING_GROW()},
                       .childAlignment = {.x = CLAY_ALIGN_X_CENTER}}}) {
        // Pan knob (visual representation)
        float pan_normalized = (track->pan + 1.0F) / 2.0F;
        CLAY({.id = CLAY_IDI("PanKnob", track_index),
              .layout = {.sizing = {CLAY_SIZING_FIXED(60),
                                    CLAY_SIZING_FIXED(20)}},
              .backgroundColor = COLOR_SLIDER_BG}) {
          float fill_width = pan_normalized * 60;
          if (fill_width > 1.0F) {
            CLAY({.id = CLAY_IDI("PanFill", track_index),
                  .layout = {.sizing = {CLAY_SIZING_FIXED(fill_width),
                                        CLAY_SIZING_FIXED(20)}},
                  .backgroundColor = COLOR_SLIDER}) {}
          }
        }

        CLAY_TEXT(
            CLAY_STRING("PAN"),
            CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM, .fontSize = 8}));
      }

      // Meters
      CLAY({.id = CLAY_IDI("Meters", track_index),
            .layout = {
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 5,
                .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(200)}}}) {
        build_meter(track->peak_level[0], track_index * 10, 200, true);
        build_meter(track->peak_level[1], track_index * 10, 200, false);
      }
    }
  }
}

static void build_master_section(AudioEngine *engine, UIState *ui_state) {
  int clicked = 0;

  CLAY(
      {.id = CLAY_ID("MasterSection"),
       .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                  .sizing = {CLAY_SIZING_PERCENT(.3), CLAY_SIZING_GROW()},
                  .padding = {10, 10, 10, 10},
                  .childGap = 10},
       .backgroundColor = COLOR_PANEL,
       .cornerRadius = CLAY_CORNER_RADIUS(4),
       .border = {.width = CLAY_BORDER_ALL(3), .color = COLOR_BUTTON_ACTIVE}}) {
    // Master label
    CLAY_TEXT(CLAY_STRING("MASTER"),
              CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT, .fontSize = 18}));

    // Master play/stop button
    bool playing = atomic_load(&engine->playing);
    clicked = 0;
    build_button(playing ? "STOP ALL" : "PLAY ALL", 0, playing, &clicked,
                 ui_state);
    /* if (clicked) */
    /*   g_ui_state->master_play_toggle = true; */

    // Master volume + meters
    CLAY({.id = CLAY_ID("MasterControls"),
          .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 10,
                     .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(300)}}}) {
      // Master volume fader
      CLAY(
          {.id = CLAY_ID("MasterVolume"),
           .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                      .childGap = 5,
                      .sizing = {CLAY_SIZING_FIXED(40), CLAY_SIZING_GROW()}}}) {
        build_vertical_fader(engine->master_volume, 9999, 250);
        CLAY_TEXT(
            CLAY_STRING("MASTER"),
            CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM, .fontSize = 8}));
      }

      // Master meters
      CLAY({.id = CLAY_ID("MasterMeters"),
            .layout = {
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 10,
                .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(250)}}}) {
        build_meter(engine->master_peak[0], 0, 250, true);
        build_meter(engine->master_peak[1], 0, 250, false);
      }
    }

    // L/R labels
    CLAY({.id = CLAY_ID("MeterLabels"),
          .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .childGap = 35,
                     .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(15)},
                     .childAlignment = {.x = CLAY_ALIGN_X_RIGHT}}}) {
      CLAY_TEXT(CLAY_STRING("L"), CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM,
                                                    .fontSize = 10}));
      CLAY_TEXT(CLAY_STRING("R"), CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM,
                                                    .fontSize = 10}));
    }
  }
}

static void build_toolbar(AudioEngine *engine, UIState *ui_state) {
  int clicked = 0;

  CLAY({.id = CLAY_ID("Toolbar"),
        .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                   .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(50)},
                   .padding = {10, 10, 10, 10},
                   .childGap = 10,
                   .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}},
        .border = {.color = COLOR_PANEL, .width = {.top = 2}}}) {
    // Add track button
    clicked = 0;
    build_button("+ ADD TRACK", 0, false, &clicked, ui_state);
    /* if (clicked) */
    /*   g_ui_state->add_track_requested = true; */

    // Status text
    char *status = malloc(sizeof(char) * 255);
    int len = sprintf(
        status, "Tracks: %d/%d | %s | %u Hz", engine->track_count, MAX_TRACKS,
        atomic_load(&engine->playing) ? "PLAYING" : "STOPPED", SAMPLE_RATE);

    Clay_String text = {
        .isStaticallyAllocated = false, .chars = status, .length = len};
    CLAY_TEXT(text,
              CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT, .fontSize = 10}));
  }
}

// ============================================================================
// UI LAYOUT
// ============================================================================

Clay_RenderCommandArray ui_build_layout(UIState *ui_state,
                                        AudioEngine *engine) {
  Clay_BeginLayout();

  CLAY({
      .id = CLAY_ID("Root"),
      .layout =
          {
              .layoutDirection = CLAY_TOP_TO_BOTTOM,
              .sizing = {CLAY_SIZING_FIXED(ui_state->window_width),
                         CLAY_SIZING_FIXED(ui_state->window_height)},
          },
      .backgroundColor = COLOR_BACKGROUND,
  }) {
    // Header
    CLAY({.id = CLAY_ID("Header"),
          .layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,
                     .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_FIXED(60)},
                     .padding = {10, 10, 10, 10},
                     .childGap = 5},
          .backgroundColor = COLOR_PANEL}) {
      CLAY_TEXT(CLAY_STRING("AirDAW"),
                CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT, .fontSize = 24}));
      CLAY_TEXT(
          CLAY_STRING("Miniaudio + Raylib + Clay UI"),
          CLAY_TEXT_CONFIG({.textColor = COLOR_TEXT_DIM, .fontSize = 10}));
    }

    // Main content area (tracks + master)
    CLAY({.id = CLAY_ID("MainContent"),
          .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                     .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                     .padding = {10, 10, 10, 10},
                     .childGap = 10}}) {
      // Tracks container (scrollable)
      CLAY({.id = CLAY_ID("TracksContainer"),
            .layout = {.layoutDirection = CLAY_LEFT_TO_RIGHT,
                       .sizing = {CLAY_SIZING_GROW(), CLAY_SIZING_GROW()},
                       .childGap = 10}}) {
        // Draw all tracks
        for (int i = 0; i < engine->track_count; i++) {
          build_track_ui(ui_state, &engine->tracks[i], i);
        }
      }

      // Master section (fixed on right)
      build_master_section(engine, ui_state);
    }

    // Toolbar at bottom
    build_toolbar(engine, ui_state);
  }

  return Clay_EndLayout();
}

// ============================================================================
// UI INTERACTION HANDLING
// ============================================================================

void ui_handle_interactions(UIState *ui_state, AudioEngine *engine) {
  // Handle track play toggle
  if (ui_state->track_play_toggle >= 0 &&
      ui_state->track_play_toggle < engine->track_count) {
    Track *track = &engine->tracks[ui_state->track_play_toggle];
    bool playing = atomic_load(&track->playing);
    atomic_store(&track->playing, !playing);
    TraceLog(LOG_INFO, "[raylib][UI] Track %d play toggled: %s",
             ui_state->track_play_toggle, !playing ? "ON" : "OFF");
  }

  // Handle track mute toggle
  if (ui_state->track_mute_toggle >= 0 &&
      ui_state->track_mute_toggle < engine->track_count) {
    Track *track = &engine->tracks[ui_state->track_mute_toggle];
    track->mute = !track->mute;
    TraceLog(LOG_INFO, "[raylib][UI] Track %d mute: %s",
             ui_state->track_mute_toggle, track->mute ? "ON" : "OFF");
  }

  // Handle track solo toggle
  if (ui_state->track_solo_toggle >= 0 &&
      ui_state->track_solo_toggle < engine->track_count) {
    Track *track = &engine->tracks[ui_state->track_solo_toggle];
    track->solo = !track->solo;
    TraceLog(LOG_INFO, "[raylib][UI] Track %d solo: %s",
             ui_state->track_solo_toggle, track->solo ? "ON" : "OFF");
  }

  // Handle master play toggle
  if (ui_state->master_play_toggle) {
    bool playing = atomic_load(&engine->playing);
    atomic_store(&engine->playing, !playing);
    TraceLog(LOG_INFO, "[raylib][UI] Master play toggled: %s",
             !playing ? "ON" : "OFF");
  }

  // Handle add track request
  if (ui_state->add_track_requested && engine->track_count < MAX_TRACKS) {
    char name[32];
    float freq = 220.0F * powf(2.0F, (float)engine->track_count / 12.0F);
    snprintf(name, sizeof(name), "Track %d", engine->track_count + 1);
    audio_engine_add_track(engine, name, freq);
  }

  // Handle add effect request
  if (ui_state->track_add_effect >= 0 &&
      ui_state->track_add_effect < engine->track_count) {
    Track *track = &engine->tracks[ui_state->track_add_effect];
    audio_engine_add_effect(track, ui_state->effect_to_add);
    TraceLog(LOG_INFO, "[raylib][UI] Added effect to track %d",
             ui_state->track_add_effect);
  }
}
