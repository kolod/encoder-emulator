// Encoder emulator for Raspberry Pi Pico
// Copyright (C) 2026-...  Oleksandr Kolodkin <oleksandr.kolodkin@ukr.net>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "pico/mutex.h"

// Shared motion state — Core 0 writes current_*, Core 1 writes everything else.
typedef struct {
    int64_t encoder_current_position;
    int32_t encoder_current_speed;
    int64_t encoder_target_position;
    int32_t encoder_target_speed;
    int32_t encoder_acceleration;
    int32_t encoder_deceleration;
    int16_t encoder_ppr;
    int32_t encoder_increment;
    int32_t encoder_target_velocity;  // signed speed setpoint used in speed mode
    bool    is_linear;
    bool    is_speed_mode;
    bool    reset_requested;
} emulated_t;

typedef struct {
    const char *label;
    int64_t     min_val;
    int64_t     max_val;
    int32_t     step;
} param_info_t;

#define PARAM_COUNT 7

extern mutex_t            emulated_mutex;
extern emulated_t         emulated;
extern const param_info_t params[PARAM_COUNT];

int64_t param_get(int i);
void    param_set(int i, int64_t v);
