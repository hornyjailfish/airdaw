#pragma once

#include "raylib.h"
#include "vendor/clay/clay.h"
#include <math.h>

#define CLAY_RECTANGLE_TO_RAYLIB_RECTANGLE(rectangle)                          \
  (Rectangle) {                                                                \
    .x = rectangle.x, .y = rectangle.y, .width = rectangle.width,              \
    .height = rectangle.height                                                 \
  }
#define CLAY_COLOR_TO_RAYLIB_COLOR(color)                                      \
  (Color) {                                                                    \
    .r = (unsigned char)roundf(color.r), .g = (unsigned char)roundf(color.g),  \
    .b = (unsigned char)roundf(color.b), .a = (unsigned char)roundf(color.a)   \
  }

#define CLAY_MEMORY_SIZE Clay_MinMemorySize()

// UI Colors
#define COLOR_BACKGROUND (Clay_Color){25, 25, 30, 255}
#define COLOR_PANEL (Clay_Color){35, 35, 40, 255}
#define COLOR_TRACK_BG (Clay_Color){45, 45, 50, 255}
#define COLOR_TRACK_BORDER (Clay_Color){60, 60, 65, 255}
#define COLOR_BUTTON (Clay_Color){70, 70, 75, 255}
#define COLOR_BUTTON_HOVER (Clay_Color){90, 90, 95, 255}
#define COLOR_BUTTON_ACTIVE (Clay_Color){50, 150, 200, 255}
#define COLOR_SLIDER (Clay_Color){50, 150, 200, 255}
#define COLOR_SLIDER_BG (Clay_Color){30, 30, 35, 255}
#define COLOR_TEXT (Clay_Color){220, 220, 225, 255}
#define COLOR_TEXT_DIM (Clay_Color){140, 140, 145, 255}
#define COLOR_METER_GREEN (Clay_Color){50, 200, 50, 255}
#define COLOR_METER_YELLOW (Clay_Color){220, 200, 50, 255}
#define COLOR_METER_RED (Clay_Color){220, 50, 50, 255}

void HandleClayErrors(Clay_ErrorData errorData);

typedef enum { CUSTOM_LAYOUT_ELEMENT_TYPE_3D_MODEL } CustomLayoutElementType;

typedef struct {
  Model model;
  float scale;
  Vector3 position;
  Matrix rotation;
} CustomLayoutElement_3DModel;

typedef struct {
  CustomLayoutElementType type;
  union {
    CustomLayoutElement_3DModel model;
  } customData;
} CustomLayoutElement;

Ray GetScreenToWorldPointWithZDistance(Vector2 position, Camera camera,
                                       int screenWidth, int screenHeight,
                                       float zDistance);

static inline Clay_Dimensions clay_measure_text(Clay_StringSlice text,
                                                Clay_TextElementConfig *config,
                                                void *userData) {
  // Measure string size for Font
  Clay_Dimensions textSize = {0};

  float maxTextWidth = 0.0F;
  float lineTextWidth = 0;
  int maxLineCharCount = 0;
  int lineCharCount = 0;

  float textHeight = config->fontSize;
  Font *fonts = (Font *)userData;
  Font fontToUse = fonts[config->fontId];
  // Font failed to load, likely the fonts are in the wrong place relative to
  // the execution dir. RayLib ships with a default font, so we can continue
  // with that built in one.
  if (!fontToUse.glyphs) {
    fontToUse = GetFontDefault();
  }

  float scaleFactor = config->fontSize / (float)fontToUse.baseSize;

  for (int i = 0; i < text.length; ++i, lineCharCount++) {
    if (text.chars[i] == '\n') {
      maxTextWidth = fmax(maxTextWidth, lineTextWidth);
      maxLineCharCount = CLAY__MAX(maxLineCharCount, lineCharCount);
      lineTextWidth = 0;
      lineCharCount = 0;
      continue;
    }
    int index = text.chars[i] - 32;
    if (fontToUse.glyphs[index].advanceX != 0)
      lineTextWidth += fontToUse.glyphs[index].advanceX;
    else
      lineTextWidth +=
          (fontToUse.recs[index].width + fontToUse.glyphs[index].offsetX);
  }

  maxTextWidth = fmax(maxTextWidth, lineTextWidth);
  maxLineCharCount = CLAY__MAX(maxLineCharCount, lineCharCount);

  textSize.width =
      maxTextWidth * scaleFactor + (lineCharCount * config->letterSpacing);
  textSize.height = textHeight;

  return textSize;
}
