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

#include "emulated.h"

mutex_t emulated_mutex;

emulated_t emulated = {
    .encoder_target_speed = 1000,
    .encoder_acceleration = 100,
    .encoder_deceleration = 100,
    .encoder_ppr          = 1000,
    .encoder_increment    = 100,
};

const param_info_t params[PARAM_COUNT] = {
    { "Tgt pos", -2000000000LL, 2000000000LL, 100 },
    { "Tgt spd", 1,             100000,        100 },
    { "Accel",   1,             100000,         10 },
    { "Decel",   1,             100000,         10 },
    { "PPR",     1,             32767,            1 },
    { "Incr",    1,             1000000,           1 },
    { "Linear",  0,             1,                1 },
};

int64_t param_get(int i) {
    mutex_enter_blocking(&emulated_mutex);
    int64_t v = 0;
    switch (i) {
        case 0: v = emulated.encoder_target_position; break;
        case 1: v = emulated.encoder_target_speed;    break;
        case 2: v = emulated.encoder_acceleration;    break;
        case 3: v = emulated.encoder_deceleration;    break;
        case 4: v = emulated.encoder_ppr;             break;
        case 5: v = emulated.encoder_increment;          break;
        case 6: v = emulated.is_linear ? 1 : 0; break;
    }
    mutex_exit(&emulated_mutex);
    return v;
}

void param_set(int i, int64_t v) {
    mutex_enter_blocking(&emulated_mutex);
    switch (i) {
        case 0: emulated.encoder_target_position = v;           break;
        case 1: emulated.encoder_target_speed    = (int32_t)v;  break;
        case 2: emulated.encoder_acceleration    = (int32_t)v;  break;
        case 3: emulated.encoder_deceleration    = (int32_t)v;  break;
        case 4: emulated.encoder_ppr             = (int16_t)v;  break;
        case 5: emulated.encoder_increment       = (int32_t)v;  break;
        case 6: emulated.is_linear     = (v != 0); break;
    }
    mutex_exit(&emulated_mutex);
}
