// main_raylib.c - AirDAW Main Entry Point
// Orchestrates Raylib, Miniaudio Audio Engine, and Clay UI

#define CLAY_IMPLEMENTATION
#include "vendor/clay/clay.h"

#include "audio_engine.h"
#include "renderer.h"
#include "renderer_utils.h"
#include "ui_clay.h"
#include <math.h>
#include <raylib.h>
#include <stdatomic.h>
#include <stdio.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define WINDOW_WIDTH 1400
#define WINDOW_HEIGHT 900
#define WINDOW_TITLE "AirDAW - Miniaudio + Raylib + Clay UI"

// ============================================================================
// RAYLIB LOGGING CALLBACK
// ============================================================================
static void raylib_log_callback(int log_level, const char *text, va_list args) {
  const char *level_str = "INFO";
  switch (log_level) {
  case LOG_FATAL:
    level_str = "\x1B[31mFATAL";
    break;
  case LOG_ERROR:
    level_str = "\x1B[31mERROR";
    break; // Red
  case LOG_WARNING:
    level_str = "\x1B[33mWARNING";
    break; // Yellow
  case LOG_DEBUG:
    level_str = "\x1B[36mDEBUG";
    break; // Cyan
  case LOG_INFO:
    level_str = "\x1B[94mINFO";
    break; // Bright Blue
  case LOG_TRACE:
    level_str = "\x1B[32mTRACE";
    break; // Green
  default:
    level_str = "\x1B[0mUNKNOWN";
    break; // Reset/Default
  }

  printf("%s\x1B[0m ", level_str);
  vprintf(text, args);
  printf("\n");
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
  // Set custom log callback for Raylib
  SetTraceLogCallback(raylib_log_callback);
  SetTraceLogLevel(LOG_INFO);

  // Initialize audio engine first (before window, so we can fail fast)
  AudioEngine engine = {0};

  if (!audio_engine_init(&engine)) {
    TraceLog(LOG_ERROR, "Failed to initialize audio engine");
    return 1;
  }
    ma_engine_config config = ma_engine_config_init();
    config.pDevice = &engine.device;
    config.pLog = &engine.log;
    ma_engine *ng = malloc(sizeof(ma_engine));
    ma_result res = ma_engine_init(&config, ng);
    if (res != MA_SUCCESS) {
        TraceLog(LOG_ERROR, "%s", ma_result_description(res));
        return 1;
    }
    if(&engine.device == ma_engine_get_device(ng)) {
        int listeners = ma_engine_get_time(ng);
        ma_log *lg = ma_engine_get_log(ng);
        ma_log_postf(lg,MA_LOG_LEVEL_WARNING, "I SEE %d", listeners);
    }
  // Add some initial tracks
  audio_engine_add_track(&engine, "Bass", 110.0F);
  audio_engine_add_track(&engine, "Lead", 440.0F);
  audio_engine_add_track(&engine, "Pad", 220.0F);

  // Add some effects to demonstrate functionality
  audio_engine_add_effect(&engine.tracks[0], EFFECT_LOWPASS);
  audio_engine_add_effect(&engine.tracks[1], EFFECT_HIGHPASS);
  audio_engine_add_effect(&engine.tracks[2], EFFECT_GAIN);

  // Initialize window
  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE);
  SetTargetFPS(144);
  MaximizeWindow();
  SetExitKey(KEY_NULL); // Disable ESC to quit (we'll handle it manually)

  TraceLog(LOG_INFO, "[raylib] Window initialized: %dx%d", WINDOW_WIDTH,
           WINDOW_HEIGHT);

  // Initialize UI system
  UIState ui_state = {0};
  if (!ui_init(&ui_state, WINDOW_WIDTH, WINDOW_HEIGHT)) {
    TraceLog(LOG_ERROR, "[raylib] Failed to initialize UI");
    CloseWindow();
    audio_engine_shutdown(&engine);
    return 1;
  }
  Clay_SetDebugModeEnabled(false);

  // Main loop
  bool should_quit = false;

  while (!WindowShouldClose() && !should_quit) {

    // Handle keyboard shortcuts
    if (IsKeyPressed(KEY_ESCAPE)) {
      should_quit = true;
    }

    if (IsKeyPressed(KEY_SPACE)) {
      bool playing = atomic_load(&engine.playing);
      atomic_store(&engine.playing, !playing);
      TraceLog(LOG_INFO, "[raylib] Master play toggled: %s",
               !playing ? "ON" : "OFF");
    }

    // Add track with 'T' key
    if (IsKeyPressed(KEY_T)) {
      if (engine.track_count < MAX_TRACKS) {
        char name[32];
        float freq = 220.0F * powf(2.0F, (float)engine.track_count / 12.0F);
        snprintf(name, sizeof(name), "Track %d", engine.track_count + 1);
        audio_engine_add_track(&engine, name, freq);
      }
    }

    // Update UI state
    ui_update(&ui_state);

    // Build UI layout
    Clay_RenderCommandArray renderCommands =
        ui_build_layout(&ui_state, &engine);

    // Handle UI interactions (button clicks, etc.)
    ui_handle_interactions(&ui_state, &engine);

    // Render
    BeginDrawing();
    ClearBackground(CLAY_COLOR_TO_RAYLIB_COLOR(COLOR_BACKGROUND));

    // Render Clay UI
    ui_render(&ui_state, renderCommands);

    TraceLog(LOG_DEBUG, "[raylib] FPS: %d", GetFPS());

    EndDrawing();
  }

  // Cleanup
  ui_shutdown(&ui_state);
  CloseWindow();
  audio_engine_shutdown(&engine);

  return 0;
}
