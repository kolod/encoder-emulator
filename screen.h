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

typedef enum { 
    EVENT_KEY_CLICK,        // Key released after a short press
    EVENT_KEY_DOWN,         // Key press event (including repeats)
    EVENT_KEY_UP,           // Key release event
    EVENT_KEY_LONG_PRESS,   // Key held down for a long duration
    EVENT_KEY_DOUBLE_PRESS, // Two quick presses of the same key
    EVENT_ENCODER_ROTATE,   // Encoder rotated (arg: rotation delta)
} event_t;

typedef enum {
    KEY_BACK,
    KEY_CONFIRM,
    KEY_ENCODER
} key_t;

typedef void (*on_draw_t)(void);
typedef void (*on_event_t)(event_t event, int arg);

typedef struct {
    screen_t *previous;
    on_draw_t on_draw;
    on_event_t on_event;
} screen_t;

static screen_t *current_screen = NULL;

// Default event handler
void default_on_event(event_t event, int arg) {
    // Check if current_screen is set before handling events
    if (!current_screen) return;

    // Check if there is a previous screen to go back to
    if (!current_screen->previous) return;

    // Handle back key event
    if (event == EVENT_KEY_DOWN && arg == KEY_BACK)
        current_screen = current_screen->previous;
}