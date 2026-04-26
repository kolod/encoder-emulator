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

#include <math.h>
#include "pico/stdlib.h"
#include "board.h"
#include "emulated.h"
#include "encoder_out.h"

// AB state → A/B levels, forward sequence: state 0→1→2→3→0
static const uint8_t qa[] = { 0, 1, 1, 0 };
static const uint8_t qb[] = { 0, 0, 1, 1 };

static void qenc_init(void) {
    const uint pins[] = { EMULATED_ENCODER_PHASE_A_PIN, EMULATED_ENCODER_PHASE_B_PIN, EMULATED_ENCODER_INDEX_PIN };
    for (int i = 0; i < 3; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_OUT);
        gpio_put(pins[i], 0);
    }
}

static void qenc_set(int64_t pos, int16_t ppr, bool linear) {
    uint8_t ph  = (uint8_t)(((pos % 4) + 4) % 4);
    bool    idx = linear ? (pos == 0) : (ppr > 0 && pos != 0 && (pos % ppr) == 0);
    gpio_put(EMULATED_ENCODER_PHASE_A_PIN, qa[ph]);
    gpio_put(EMULATED_ENCODER_PHASE_B_PIN, qb[ph]);
    gpio_put(EMULATED_ENCODER_INDEX_PIN,   idx);
}

// Trapezoidal motion profile using kinematic v² = u² ± 2a (one step = 1 unit).
void core0_entry(void) {
    qenc_init();

    int64_t pos   = 0;
    int32_t speed = 0; // steps/sec, signed

    while (true) {
        mutex_enter_blocking(&emulated_mutex);
        if (emulated.reset_requested) {
            pos = 0; speed = 0;
            emulated.encoder_current_position = 0;
            emulated.encoder_current_speed    = 0;
            emulated.reset_requested          = false;
        }
        int64_t target = emulated.encoder_target_position;
        int32_t tspeed = emulated.encoder_target_speed;
        int32_t accel  = emulated.encoder_acceleration;
        int32_t decel  = emulated.encoder_deceleration;
        int16_t ppr    = emulated.encoder_ppr;
        bool    linear = emulated.is_linear;
        mutex_exit(&emulated_mutex);

        int64_t delta = target - pos;

        if (delta == 0 && speed == 0) {
            sleep_ms(5);
            continue;
        }

        int32_t dir     = (delta > 0) ? 1 : -1;
        int64_t dist    = delta < 0 ? -delta : delta;
        int32_t abs_spd = speed < 0 ? -speed : speed;

        // Stopping distance at current speed: v²/(2a)
        int64_t stop_dist = (decel > 0) ? ((int64_t)abs_spd * abs_spd) / (2 * decel) : 0;

        // Decelerate if we'd overshoot, or if moving in the wrong direction
        bool decel_phase = (dist <= stop_dist + 1) || (dir > 0 && speed < 0) || (dir < 0 && speed > 0);

        int64_t sq;
        if (decel_phase) {
            sq = (int64_t)abs_spd * abs_spd - 2 * decel;
            abs_spd = (sq > 0) ? (int32_t)sqrtf((float)sq) : 0;
        } else {
            sq = (int64_t)abs_spd * abs_spd + 2 * accel;
            abs_spd = (int32_t)sqrtf((float)sq);
            if (abs_spd > tspeed) abs_spd = tspeed;
        }
        if (abs_spd == 0 && dist > 0) abs_spd = 1;
        speed = dir * abs_spd;

        pos += dir;
        if ((dir > 0 && pos > target) || (dir < 0 && pos < target)) {
            pos = target; speed = 0;
        }

        qenc_set(pos, ppr, linear);

        mutex_enter_blocking(&emulated_mutex);
        emulated.encoder_current_position = pos;
        emulated.encoder_current_speed    = speed;
        mutex_exit(&emulated_mutex);

        uint32_t step_us = abs_spd > 0 ? (1000000u / (uint32_t)abs_spd) : 1000000u;
        sleep_us(step_us);
    }
}
