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

// Waveshare RP2040-Zero Pin Definitions

// Onboard Smart LED
#define WS2812_LED_PIN  16
#define WS2812_LED_FREQ 800000.f

// 1.3" OLED Display (I2C) with buttons and encoder Pin Definitions

// OLED Display (I2C)
#define OLED_SDA_PIN  0
#define OLED_SCL_PIN  1
#define OLED_I2C_PORT i2c0
#define OLED_I2C_FREQ 400000  // 400 kHz
#define OLED_I2C_ADDR 0x3C    // SSD1306 I2C address

// Buttons on the Display board
#define DISPLAY_BUTTON_BACK_PIN        3
#define DISPLAY_BUTTON_CONFIRM_PIN     4

// Rotary Encoder (Input)
#define DISPLAY_ENCODER_PUSH_PIN       5
#define DISPLAY_ENCODER_PHASE_A_PIN    6
#define DISPLAY_ENCODER_PHASE_B_PIN    7

// Quadrature Encoder (Output)
#define EMULATED_ENCODER_PHASE_A_PIN   8
#define EMULATED_ENCODER_PHASE_B_PIN   9
#define EMULATED_ENCODER_INDEX_PIN    10