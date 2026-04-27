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
#include "pico/multicore.h"
#include "ws2812.pio.h"
#include "board.h"
#include "emulated.h"
#include "display.h"
#include "input.h"
#include "serial.h"
#include "encoder_out.h"

static void core1_entry(void) {
    display_init();

    static debounce_t dbounce;
    uint32_t prev_state = debounce_setup(&dbounce, pio1,
                                         DISPLAY_BUTTON_BACK_PIN, 5,
                                         DEBOUNCE_SETTLE_US);
    input_irq_init(&dbounce);
    uint32_t last_draw_ms = 0;

    while (true) {
        serial_poll();

        event_t ev = input_poll(&prev_state);
        if (ev != EVENT_NONE) handle_event(ev);

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_draw_ms >= 50) {
            last_draw_ms = now;
            draw_screen();
        }

        sleep_ms(10);
    }
}

int main(void) {
    stdio_init_all();
    mutex_init(&emulated_mutex);
    ws2812_init();
    ws2812_set_rgb(0, 0, 16);
    multicore_launch_core1(core1_entry);
    core0_entry();
}
