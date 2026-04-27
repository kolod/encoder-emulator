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

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "u8g2.h"
#include "board.h"
#include "emulated.h"
#include "display.h"

// ---- Screen state -------------------------------------------------------

typedef enum { NAV_ROOT, NAV_MENU, NAV_PARAMS } nav_t;

static nav_t nav        = NAV_ROOT;
static int   menu_sel   = 0;
static int   param_sel    = 0;
static int   param_scroll = 0;
static bool  param_edit   = false;

// ---- I2C / u8g2 callbacks -----------------------------------------------

static uint8_t  i2c_buf[256];
static uint16_t i2c_buf_len;

static uint8_t u8x8_pico_i2c_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg, void *argp) {
    switch (msg) {
        case U8X8_MSG_BYTE_START_TRANSFER: i2c_buf_len = 0; break;
        case U8X8_MSG_BYTE_SEND: {
            const uint8_t *p = (const uint8_t *)argp;
            while (arg--) i2c_buf[i2c_buf_len++] = *p++;
            break;
        }
        case U8X8_MSG_BYTE_END_TRANSFER:
            i2c_write_blocking(OLED_I2C_PORT, u8x8_GetI2CAddress(u8x8) >> 1, i2c_buf, i2c_buf_len, false);
            break;
        case U8X8_MSG_BYTE_INIT:
        case U8X8_MSG_BYTE_SET_DC: break;
        default: return 0;
    }
    return 1;
}

static uint8_t u8x8_pico_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg, void *argp) {
    (void)u8x8; (void)argp;
    switch (msg) {
        case U8X8_MSG_DELAY_100NANO: __asm volatile("nop"); break;
        case U8X8_MSG_DELAY_10MICRO: sleep_us(10);          break;
        case U8X8_MSG_DELAY_MILLI:   sleep_ms(arg);         break;
        case U8X8_MSG_DELAY_I2C:     sleep_us(5);           break;
        case U8X8_MSG_GPIO_I2C_CLOCK:
        case U8X8_MSG_GPIO_I2C_DATA: break;
        default: return 0;
    }
    return 1;
}

// ---- Init ---------------------------------------------------------------

static u8g2_t u8g2;

void display_init(void) {
    i2c_init(OLED_I2C_PORT, OLED_I2C_FREQ);
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);
    u8g2_Setup_sh1106_i2c_128x64_noname_f(&u8g2, U8G2_R0, u8x8_pico_i2c_cb, u8x8_pico_delay_cb);
    u8g2_SetI2CAddress(&u8g2, (uint8_t)(OLED_I2C_ADDR << 1));
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetFont(&u8g2, u8g2_font_6x10_tf);
}

// ---- Draw ---------------------------------------------------------------

// 128×64, font 6×10 (baseline y): 6 lines at y=10,20,30,40,50,60

static void draw_root(void) {
    mutex_enter_blocking(&emulated_mutex);
    long cur      = (long)emulated.encoder_current_position;
    long tgt      = (long)emulated.encoder_target_position;
    long spd      = (long)emulated.encoder_current_speed;
    long tvel     = (long)emulated.encoder_target_velocity;
    bool spd_mode = emulated.is_speed_mode;
    mutex_exit(&emulated_mutex);

    char buf[24];
    snprintf(buf, sizeof(buf), "%ld", cur);
    u8g2_DrawStr(&u8g2, 0, 10, "Current pos:");
    u8g2_DrawStr(&u8g2, 0, 20, buf);

    if (spd_mode) {
        snprintf(buf, sizeof(buf), "%ld", tvel);
        u8g2_DrawStr(&u8g2, 0, 30, "Target spd:");
        u8g2_DrawStr(&u8g2, 0, 40, buf);

        snprintf(buf, sizeof(buf), "%ld", spd);
        u8g2_DrawStr(&u8g2, 0, 50, "Cur speed:");
        u8g2_DrawStr(&u8g2, 0, 60, buf);
    } else {
        snprintf(buf, sizeof(buf), "%ld", tgt);
        u8g2_DrawStr(&u8g2, 0, 30, "Target pos:");
        u8g2_DrawStr(&u8g2, 0, 40, buf);

        snprintf(buf, sizeof(buf), "%ld", spd);
        u8g2_DrawStr(&u8g2, 0, 50, "Speed:");
        u8g2_DrawStr(&u8g2, 0, 60, buf);
    }
}

static void draw_menu(void) {
    mutex_enter_blocking(&emulated_mutex);
    bool spd_mode = emulated.is_speed_mode;
    mutex_exit(&emulated_mutex);
    char mode_label[16];
    snprintf(mode_label, sizeof(mode_label), "Mode: %s", spd_mode ? "Speed" : "Pos");
    const char *items[] = { "Reset position", mode_label, "Parameters" };
    for (int i = 0; i < 3; i++) {
        char buf[22];
        snprintf(buf, sizeof(buf), "%c %s", (i == menu_sel) ? '>' : ' ', items[i]);
        u8g2_DrawStr(&u8g2, 0, 22 + i * 12, buf);
    }
}

static void draw_params(void) {
    mutex_enter_blocking(&emulated_mutex);
    bool spd_mode = emulated.is_speed_mode;
    mutex_exit(&emulated_mutex);

    int row = 0;
    for (int i = param_scroll; i < PARAM_COUNT && row < 6; i++) {
        if (spd_mode && i == 0) continue;

        int64_t v = param_get(i);
        char vbuf[14];
        if (i == 6) snprintf(vbuf, sizeof(vbuf), "%s", v ? "Yes" : "No");
        else        snprintf(vbuf, sizeof(vbuf), "%ld", (long)v);

        char buf[24];
        bool sel = (i == param_sel);
        if (sel && param_edit)
            snprintf(buf, sizeof(buf), ">%-7s[%s]", params[i].label, vbuf);
        else
            snprintf(buf, sizeof(buf), "%c%-7s %s", sel ? '>' : ' ', params[i].label, vbuf);
        u8g2_DrawStr(&u8g2, 0, 10 + row * 10, buf);
        row++;
    }
}

void draw_screen(void) {
    u8g2_ClearBuffer(&u8g2);
    switch (nav) {
        case NAV_ROOT:   draw_root();   break;
        case NAV_MENU:   draw_menu();   break;
        case NAV_PARAMS: draw_params(); break;
    }
    u8g2_SendBuffer(&u8g2);
}

// ---- Event handler ------------------------------------------------------

void handle_event(event_t ev) {
    switch (nav) {

        case NAV_ROOT:
            if (ev == EVENT_ENCODER_PLUS || ev == EVENT_ENCODER_MINUS) {
                mutex_enter_blocking(&emulated_mutex);
                int32_t inc = emulated.encoder_increment;
                if (emulated.is_speed_mode) {
                    int32_t tspd = emulated.encoder_target_speed;
                    int32_t tvel = emulated.encoder_target_velocity;
                    tvel += (ev == EVENT_ENCODER_PLUS) ? inc : -inc;
                    if (tvel >  tspd) tvel =  tspd;
                    if (tvel < -tspd) tvel = -tspd;
                    emulated.encoder_target_velocity = tvel;
                } else {
                    emulated.encoder_target_position +=
                        (ev == EVENT_ENCODER_PLUS) ? inc : -inc;
                }
                mutex_exit(&emulated_mutex);
            }
            if (ev == EVENT_CONFIRM) { nav = NAV_MENU; menu_sel = 0; }
            break;

        case NAV_MENU:
            if (ev == EVENT_ENCODER_MINUS && menu_sel > 0) menu_sel--;
            if (ev == EVENT_ENCODER_PLUS  && menu_sel < 2) menu_sel++;
            if (ev == EVENT_BACK) nav = NAV_ROOT;
            if (ev == EVENT_CONFIRM) {
                if (menu_sel == 0) {
                    mutex_enter_blocking(&emulated_mutex);
                    emulated.encoder_target_position = 0;
                    emulated.encoder_target_velocity = 0;
                    emulated.reset_requested         = true;
                    mutex_exit(&emulated_mutex);
                    nav = NAV_ROOT;
                } else if (menu_sel == 1) {
                    mutex_enter_blocking(&emulated_mutex);
                    emulated.is_speed_mode = !emulated.is_speed_mode;
                    mutex_exit(&emulated_mutex);
                } else {
                    mutex_enter_blocking(&emulated_mutex);
                    bool spd = emulated.is_speed_mode;
                    mutex_exit(&emulated_mutex);
                    nav = NAV_PARAMS; param_sel = spd ? 1 : 0; param_scroll = 0; param_edit = false;
                }
            }
            break;

        case NAV_PARAMS: {
            mutex_enter_blocking(&emulated_mutex);
            bool spd_mode = emulated.is_speed_mode;
            mutex_exit(&emulated_mutex);
            int min_sel = spd_mode ? 1 : 0;
            if (!param_edit) {
                if (ev == EVENT_ENCODER_MINUS && param_sel > min_sel) {
                    param_sel--;
                    if (param_sel < param_scroll) param_scroll = param_sel;
                }
                if (ev == EVENT_ENCODER_PLUS  && param_sel < PARAM_COUNT - 1) {
                    param_sel++;
                    if (param_sel >= param_scroll + 6) param_scroll = param_sel - 5;
                }
                if (ev == EVENT_BACK) nav = NAV_MENU;
                if (ev == EVENT_CONFIRM) {
                    if (param_sel == 6)
                        param_set(6, param_get(6) ^ 1);
                    else
                        param_edit = true;
                }
            } else {
                if (ev == EVENT_ENCODER_PLUS || ev == EVENT_ENCODER_MINUS) {
                    int64_t v = param_get(param_sel);
                    v += (ev == EVENT_ENCODER_PLUS) ? params[param_sel].step : -params[param_sel].step;
                    if (v < params[param_sel].min_val) v = params[param_sel].min_val;
                    if (v > params[param_sel].max_val) v = params[param_sel].max_val;
                    param_set(param_sel, v);
                }
                if (ev == EVENT_CONFIRM || ev == EVENT_BACK) param_edit = false;
            }
            break;
        }
    }
}
