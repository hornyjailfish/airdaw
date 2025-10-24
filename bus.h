#pragma once

#include "vendor/miniaudio/miniaudio.h"
#include <stdatomic.h>
#include <stdbool.h>

typedef struct {
    ma_data_source_node source;
    ma_sound_group group;
    ma_data_source_node_config config;
}Bus;
