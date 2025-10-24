#pragma once
#include "ui_clay.h"
#include <stdbool.h>


bool ui_init(UIState *ui_state, int window_width, int window_height);

void ui_shutdown(UIState *ui_state);

void ui_update(UIState *ui_state);

void ui_render(UIState *ui_state, Clay_RenderCommandArray renderCommands);
