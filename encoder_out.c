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
// Float velocity avoids integer stall: with int, sqrt(v²+2a)-v ≈ a/v < 1 once v≥a.
void core0_entry(void) {
    qenc_init();

    int64_t pos    = 0;
    float   fspeed = 0.0f;  // steps/sec, signed

    while (true) {
        mutex_enter_blocking(&emulated_mutex);
        if (emulated.reset_requested) {
            pos = 0; fspeed = 0.0f;
            emulated.encoder_current_position = 0;
            emulated.encoder_current_speed    = 0;
            emulated.encoder_target_velocity  = 0;
            emulated.reset_requested          = false;
        }
        int64_t target   = emulated.encoder_target_position;
        float   ftspeed  = (float)emulated.encoder_target_speed;
        float   ftvel    = (float)emulated.encoder_target_velocity;
        float   fa       = (float)emulated.encoder_acceleration;
        float   fd       = (float)emulated.encoder_deceleration;
        int16_t ppr      = emulated.encoder_ppr;
        bool    linear   = emulated.is_linear;
        bool    spd_mode = emulated.is_speed_mode;
        mutex_exit(&emulated_mutex);

        float abs_spd = fspeed < 0.0f ? -fspeed : fspeed;

        if (spd_mode) {
            float abs_tvel = ftvel < 0.0f ? -ftvel : ftvel;

            if (abs_spd < 0.5f && abs_tvel < 0.5f) {
                sleep_ms(5);
                continue;
            }

            int32_t dir = (fspeed > 0.5f) ? 1 : (fspeed < -0.5f) ? -1 : (ftvel >= 0.0f) ? 1 : -1;
            bool reversing = (abs_spd > 0.5f) && (abs_tvel > 0.5f) &&
                             ((ftvel > 0.0f) != (fspeed > 0.0f));

            if (reversing) {
                float sq = abs_spd * abs_spd - 2.0f * fd;
                abs_spd = (sq > 0.0f) ? sqrtf(sq) : 0.0f;
            } else if (abs_spd < abs_tvel) {
                abs_spd = sqrtf(abs_spd * abs_spd + 2.0f * fa);
                if (abs_spd > abs_tvel) abs_spd = abs_tvel;
            } else if (abs_spd > abs_tvel) {
                float sq = abs_spd * abs_spd - 2.0f * fd;
                abs_spd = (sq > 0.0f) ? sqrtf(sq) : 0.0f;
            }
            fspeed = (float)dir * abs_spd;

            if (abs_spd < 0.5f) {
                fspeed = 0.0f;
                mutex_enter_blocking(&emulated_mutex);
                emulated.encoder_current_speed = 0;
                mutex_exit(&emulated_mutex);
                sleep_ms(5);
                continue;
            }

            pos += dir;
            qenc_set(pos, ppr, linear);

            mutex_enter_blocking(&emulated_mutex);
            emulated.encoder_current_position = pos;
            emulated.encoder_current_speed    = (int32_t)fspeed;
            mutex_exit(&emulated_mutex);

            sleep_us((uint32_t)(1000000.0f / abs_spd));
        } else {
            int64_t delta = target - pos;

            if (delta == 0 && abs_spd < 0.5f) {
                fspeed = 0.0f;
                sleep_ms(5);
                continue;
            }

            int32_t dir  = (delta > 0) ? 1 : -1;
            float   dist = (float)(delta < 0 ? -delta : delta);

            float stop_dist = (fd > 0.0f) ? (abs_spd * abs_spd) / (2.0f * fd) : 0.0f;
            bool decel_phase = (dist <= stop_dist + 1.0f) ||
                               (dir > 0 && fspeed < 0.0f) || (dir < 0 && fspeed > 0.0f);

            if (decel_phase) {
                float sq = abs_spd * abs_spd - 2.0f * fd;
                abs_spd = (sq > 0.0f) ? sqrtf(sq) : 0.0f;
            } else {
                abs_spd = sqrtf(abs_spd * abs_spd + 2.0f * fa);
                if (abs_spd > ftspeed) abs_spd = ftspeed;
            }
            if (abs_spd < 0.5f && dist > 0.0f) abs_spd = 1.0f;
            fspeed = (float)dir * abs_spd;

            pos += dir;
            if ((dir > 0 && pos > target) || (dir < 0 && pos < target)) {
                pos = target; fspeed = 0.0f; abs_spd = 0.0f;
            }

            qenc_set(pos, ppr, linear);

            mutex_enter_blocking(&emulated_mutex);
            emulated.encoder_current_position = pos;
            emulated.encoder_current_speed    = (int32_t)fspeed;
            mutex_exit(&emulated_mutex);

            uint32_t step_us = abs_spd > 0.5f ? (uint32_t)(1000000.0f / abs_spd) : 1000000u;
            sleep_us(step_us);
        }
    }
}
