#include "raylib.h"
#include "renderer_utils.h"
#include "ui_clay.h"
#include "vendor/clay/clay.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

Camera Raylib_camera;

bool ui_init(UIState *ui_state, int window_width, int window_height) {
  memset(ui_state, 0, sizeof(UIState));

  ui_state->clay_memory = malloc(CLAY_MEMORY_SIZE);
  ui_state->window_width = window_width;
  ui_state->window_height = window_height;
  ui_state->track_play_toggle = -1;
  ui_state->track_mute_toggle = -1;
  ui_state->track_solo_toggle = -1;
  ui_state->track_add_effect = -1;

  TraceLog(LOG_INFO, "[raylib][UI] Initializing UI system...");

  // TODO: separate font loading and make it proper array
  const char *font_path = "C:/Users/5q/AppData/Local/Microsoft/Windows/Fonts/"
                          "MesloLGLDZNerdFont-Regular.ttf";
  ui_state->font_count = 1;
  ui_state->font = malloc(sizeof(Font) * ui_state->font_count);
  ui_state->font[0] = LoadFont(font_path);
  if (ui_state->font[0].texture.id == 0) {
    TraceLog(LOG_WARNING, "[raylib][UI] Failed to load font, using default");
    ui_state->font[0] = GetFontDefault();
  } else {
    TraceLog(LOG_INFO, "[raylib][UI] Loaded font: %s", font_path);
    SetTextureFilter(ui_state->font[0].texture, TEXTURE_FILTER_BILINEAR);
  }

  // Initialize Clay
  ui_state->clay_arena = Clay_CreateArenaWithCapacityAndMemory(
      CLAY_MEMORY_SIZE, ui_state->clay_memory);

  Clay_Initialize(ui_state->clay_arena,
                  (Clay_Dimensions){(float)window_width, (float)window_height},
                  (Clay_ErrorHandler){HandleClayErrors, NULL});
  Clay_SetMeasureTextFunction(
      clay_measure_text,
      ui_state->font); // maybe add userdata with context/state here too

  TraceLog(LOG_INFO, "[raylib][UI] Clay initialized with %d bytes",
           CLAY_MEMORY_SIZE);
  TraceLog(LOG_INFO, "[raylib][UI] UI system initialized successfully");

  return true;
}

void ui_shutdown(UIState *ui_state) {
  TraceLog(LOG_INFO, "[raylib][UI] Shutting down UI system");

  for (uint32_t i = 0; i > ui_state->font_count; ++i) {
    UnloadFont(ui_state->font[i]);
  }
}

// ============================================================================
// UI UPDATE
// ============================================================================

void ui_update(UIState *ui_state) {
  // Update mouse state
  ui_state->mouse_pos = GetMousePosition();
  ui_state->mouse_pressed = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
  ui_state->mouse_down = IsMouseButtonDown(MOUSE_LEFT_BUTTON);
  ui_state->mouse_released = IsMouseButtonReleased(MOUSE_LEFT_BUTTON);

  // Reset per-frame action flags
  ui_state->add_track_requested = false;
  ui_state->track_play_toggle = -1;
  ui_state->track_mute_toggle = -1;
  ui_state->track_solo_toggle = -1;
  ui_state->master_play_toggle = false;
  ui_state->track_add_effect = -1;

  if (IsWindowResized()) {
    ui_state->window_width = GetScreenWidth();
    ui_state->window_height = GetScreenHeight();
  }

  Clay_SetLayoutDimensions((Clay_Dimensions){(float)ui_state->window_width,
                                             (float)ui_state->window_height});
  // Update Clay pointer state
  Clay_SetPointerState(
      (Clay_Vector2){ui_state->mouse_pos.x, ui_state->mouse_pos.y},
      ui_state->mouse_down);

  // Update Clay layout
  Clay_UpdateScrollContainers(false,                // isPointerActive
                              (Clay_Vector2){0, 0}, // scrollDelta
                              GetFrameTime());
}

void ui_render(UIState *ui_state, Clay_RenderCommandArray renderCommands) {
  for (int j = 0; j < renderCommands.length; j++) {
    Clay_RenderCommand *renderCommand =
        Clay_RenderCommandArray_Get(&renderCommands, j);
    Clay_BoundingBox boundingBox = {roundf(renderCommand->boundingBox.x),
                                    roundf(renderCommand->boundingBox.y),
                                    roundf(renderCommand->boundingBox.width),
                                    roundf(renderCommand->boundingBox.height)};
    switch (renderCommand->commandType) {
    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
      Clay_TextRenderData *textData = &renderCommand->renderData.text;
      Font fontToUse = ui_state->font[textData->fontId];

      DrawTextEx(fontToUse, textData->stringContents.baseChars,
                 (Vector2){boundingBox.x, boundingBox.y},
                 (float)textData->fontSize, (float)textData->letterSpacing,
                 CLAY_COLOR_TO_RAYLIB_COLOR(textData->textColor));
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_IMAGE: {
      Texture2D imageTexture =
          *(Texture2D *)renderCommand->renderData.image.imageData;
      Clay_Color tintColor = renderCommand->renderData.image.backgroundColor;
      if (tintColor.r == 0 && tintColor.g == 0 && tintColor.b == 0 &&
          tintColor.a == 0) {
        tintColor = (Clay_Color){255, 255, 255, 255};
      }
      DrawTexturePro(imageTexture,
                     (Rectangle){0.F, 0.F, (float)imageTexture.width,
                                 (float)imageTexture.height},
                     (Rectangle){boundingBox.x, boundingBox.y,
                                 boundingBox.width, boundingBox.height},
                     (Vector2){}, 0, CLAY_COLOR_TO_RAYLIB_COLOR(tintColor));
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START: {
      BeginScissorMode((int)roundf(boundingBox.x), (int)roundf(boundingBox.y),
                       (int)roundf(boundingBox.width),
                       (int)roundf(boundingBox.height));
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END: {
      EndScissorMode();
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
      Clay_RectangleRenderData *config = &renderCommand->renderData.rectangle;
      if (config->cornerRadius.topLeft > 0) {
        float radius =
            (config->cornerRadius.topLeft * 2) /
            ((boundingBox.width > boundingBox.height) ? boundingBox.height
                                                      : boundingBox.width);
        DrawRectangleRounded(
            (Rectangle){boundingBox.x, boundingBox.y, boundingBox.width,
                        boundingBox.height},
            radius, 8, CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
      } else {
        DrawRectangle((int)boundingBox.x, (int)boundingBox.y,
                      (int)boundingBox.width, (int)boundingBox.height,
                      CLAY_COLOR_TO_RAYLIB_COLOR(config->backgroundColor));
      }
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_BORDER: {
      Clay_BorderRenderData *config = &renderCommand->renderData.border;
      // Left border
      if (config->width.left > 0) {
        DrawRectangle((int)roundf(boundingBox.x),
                      (int)roundf(boundingBox.y + config->cornerRadius.topLeft),
                      (int)config->width.left,
                      (int)roundf(boundingBox.height -
                                  config->cornerRadius.topLeft -
                                  config->cornerRadius.bottomLeft),
                      CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      // Right border
      if (config->width.right > 0) {
        DrawRectangle(
            (int)roundf(boundingBox.x + boundingBox.width -
                        (float)config->width.right),
            (int)roundf(boundingBox.y + config->cornerRadius.topRight),
            (int)config->width.right,
            (int)roundf(boundingBox.height - config->cornerRadius.topRight -
                        config->cornerRadius.bottomRight),
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      // Top border
      if (config->width.top > 0) {
        DrawRectangle(
            (int)roundf(boundingBox.x + config->cornerRadius.topLeft),
            (int)roundf(boundingBox.y),
            (int)roundf(boundingBox.width - config->cornerRadius.topLeft -
                        config->cornerRadius.topRight),
            (int)config->width.top, CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      // Bottom border
      if (config->width.bottom > 0) {
        DrawRectangle(
            (int)roundf(boundingBox.x + config->cornerRadius.bottomLeft),
            (int)roundf(boundingBox.y + boundingBox.height -
                        (float)config->width.bottom),
            (int)roundf(boundingBox.width - config->cornerRadius.bottomLeft -
                        config->cornerRadius.bottomRight),
            (int)config->width.bottom,
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      if (config->cornerRadius.topLeft > 0) {
        DrawRing(
            (Vector2){roundf(boundingBox.x + config->cornerRadius.topLeft),
                      roundf(boundingBox.y + config->cornerRadius.topLeft)},
            roundf(config->cornerRadius.topLeft - (float)config->width.top),
            config->cornerRadius.topLeft, 180, 270, 10,
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      if (config->cornerRadius.topRight > 0) {
        DrawRing(
            (Vector2){roundf(boundingBox.x + boundingBox.width -
                             config->cornerRadius.topRight),
                      roundf(boundingBox.y + config->cornerRadius.topRight)},
            roundf(config->cornerRadius.topRight - (float)config->width.top),
            config->cornerRadius.topRight, 270, 360, 10,
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      if (config->cornerRadius.bottomLeft > 0) {
        DrawRing(
            (Vector2){roundf(boundingBox.x + config->cornerRadius.bottomLeft),
                      roundf(boundingBox.y + boundingBox.height -
                             config->cornerRadius.bottomLeft)},
            roundf(config->cornerRadius.bottomLeft -
                   (float)config->width.bottom),
            config->cornerRadius.bottomLeft, 90, 180, 10,
            CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      if (config->cornerRadius.bottomRight > 0) {
        DrawRing((Vector2){roundf(boundingBox.x + boundingBox.width -
                                  config->cornerRadius.bottomRight),
                           roundf(boundingBox.y + boundingBox.height -
                                  config->cornerRadius.bottomRight)},
                 roundf(config->cornerRadius.bottomRight -
                        (float)config->width.bottom),
                 config->cornerRadius.bottomRight, 0.1F, 90, 10,
                 CLAY_COLOR_TO_RAYLIB_COLOR(config->color));
      }
      break;
    }
    case CLAY_RENDER_COMMAND_TYPE_CUSTOM: {
      Clay_CustomRenderData *config = &renderCommand->renderData.custom;
      CustomLayoutElement *customElement =
          (CustomLayoutElement *)config->customData;
      if (!customElement) {
        continue;
      }
      switch (customElement->type) {
      case CUSTOM_LAYOUT_ELEMENT_TYPE_3D_MODEL: {
        Clay_BoundingBox rootBox = renderCommands.internalArray[0].boundingBox;
        float scaleValue = CLAY__MIN(CLAY__MIN(1, 768 / rootBox.height) *
                                         CLAY__MAX(1, rootBox.width / 1024),
                                     1.5F);
        Ray positionRay = GetScreenToWorldPointWithZDistance(
            (Vector2){renderCommand->boundingBox.x +
                          (renderCommand->boundingBox.width / 2),
                      renderCommand->boundingBox.y +
                          (renderCommand->boundingBox.height / 2) + 20},
            Raylib_camera, (int)roundf(rootBox.width),
            (int)roundf(rootBox.height), 140);
        BeginMode3D(Raylib_camera);
        DrawModel(customElement->customData.model.model, positionRay.position,
                  customElement->customData.model.scale * scaleValue,
                  WHITE); // Draw 3d model with texture
        EndMode3D();
        break;
      }
      default:
        break;
      }
      break;
    }
    default: {
      TraceLog(LOG_ERROR, "[clay] Error: unhandled render command.");
      exit(1);
    }
    }
  }
}
