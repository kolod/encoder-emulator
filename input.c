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

#include "pico/stdlib.h"
#include "board.h"
#include "input.h"

void buttons_init(void) {
    const uint pins[] = { DISPLAY_BUTTON_BACK_PIN, DISPLAY_BUTTON_CONFIRM_PIN, DISPLAY_ENCODER_PUSH_PIN };
    for (int i = 0; i < 3; i++) {
        gpio_init(pins[i]);
        gpio_set_dir(pins[i], GPIO_IN);
        gpio_pull_up(pins[i]);
    }
}

event_t input_poll(encoder_t *enc, int32_t *enc_prev,
                   bool *back_prev, bool *confirm_prev, bool *push_prev) {
    encoder_poll(enc);
    event_t ev = EVENT_NONE;

    int32_t ep = enc->position;
    if      (ep > *enc_prev) { ev = EVENT_ENC_UP;   *enc_prev = ep; }
    else if (ep < *enc_prev) { ev = EVENT_ENC_DOWN;  *enc_prev = ep; }

    bool back    = !gpio_get(DISPLAY_BUTTON_BACK_PIN);
    bool confirm = !gpio_get(DISPLAY_BUTTON_CONFIRM_PIN);
    bool push    = !gpio_get(DISPLAY_ENCODER_PUSH_PIN);

    if (ev == EVENT_NONE) {
        if (back    && !*back_prev)    ev = EVENT_BACK;
        if (confirm && !*confirm_prev) ev = EVENT_CONFIRM;
        if (push    && !*push_prev)    ev = EVENT_ENC_PUSH;
    }

    *back_prev    = back;
    *confirm_prev = confirm;
    *push_prev    = push;
    return ev;
}
