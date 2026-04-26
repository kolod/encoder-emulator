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

// Commands (newline-terminated, case-sensitive):
//   pos <n>      set target position   (steps)
//   spd <n>      set target speed      (steps/s)
//   accel <n>    set acceleration      (steps/s²)
//   decel <n>    set deceleration      (steps/s²)
//   ppr <n>      set pulses/revolution
//   linear <0|1> set mode  0=rotary  1=linear
//   reset        move to 0 and zero current position immediately
//   get          print all params and current state
//   boot         reboot into BOOTSEL (programming) mode
//   help / ?     show this list

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "emulated.h"
#include "serial.h"

static const struct { const char *cmd; int param_idx; } serial_cmds[] = {
    { "pos",    0 },
    { "spd",    1 },
    { "accel",  2 },
    { "decel",  3 },
    { "ppr",    4 },
    { "linear", 5 },
};

static void serial_handle(const char *line) {
    while (*line == ' ') line++;
    if (*line == '\0') return;

    char kw[16] = {0};
    int  ki = 0;
    while (*line && *line != ' ' && ki < 15) kw[ki++] = *line++;
    while (*line == ' ') line++;

    if (!strcmp(kw, "reset")) {
        mutex_enter_blocking(&emulated_mutex);
        emulated.encoder_target_position = 0;
        emulated.reset_requested         = true;
        mutex_exit(&emulated_mutex);
        printf("ok: position reset to 0\r\n");
        return;
    }
    if (!strcmp(kw, "get")) {
        mutex_enter_blocking(&emulated_mutex);
        long cur  = (long)emulated.encoder_current_position;
        long spd  = (long)emulated.encoder_current_speed;
        long tgt  = (long)emulated.encoder_target_position;
        long tspd = (long)emulated.encoder_target_speed;
        long acc  = (long)emulated.encoder_acceleration;
        long dec  = (long)emulated.encoder_deceleration;
        long ppr  = (long)emulated.encoder_ppr;
        bool lin  = emulated.is_linear;
        mutex_exit(&emulated_mutex);
        printf("cur=%ld spd=%ld\r\n", cur, spd);
        printf("pos=%ld spd=%ld accel=%ld decel=%ld ppr=%ld linear=%d\r\n",
               tgt, tspd, acc, dec, ppr, lin ? 1 : 0);
        return;
    }
    if (!strcmp(kw, "boot")) {
        printf("Rebooting into BOOTSEL mode...\r\n");
        reset_usb_boot(0, 0);
    }
    if (!strcmp(kw, "help") || !strcmp(kw, "?")) {
        printf("Commands:\r\n"
               "  pos <n>      target position (steps)\r\n"
               "  spd <n>      target speed (steps/s)\r\n"
               "  accel <n>    acceleration (steps/s^2)\r\n"
               "  decel <n>    deceleration (steps/s^2)\r\n"
               "  ppr <n>      pulses per revolution\r\n"
               "  linear <0|1> 0=rotary 1=linear\r\n"
               "  reset        zero position immediately\r\n"
               "  get          print current state\r\n"
               "  boot         reboot into BOOTSEL (programming) mode\r\n");
        return;
    }

    char *end;
    long long val = strtoll(line, &end, 10);
    if (end == line) { printf("err: missing value\r\n"); return; }

    for (int i = 0; i < (int)(sizeof(serial_cmds) / sizeof(*serial_cmds)); i++) {
        if (!strcmp(kw, serial_cmds[i].cmd)) {
            int pi = serial_cmds[i].param_idx;
            if (val < params[pi].min_val || val > params[pi].max_val) {
                printf("err: %s range [%ld..%ld]\r\n", kw,
                       (long)params[pi].min_val, (long)params[pi].max_val);
            } else {
                param_set(pi, (int64_t)val);
                printf("ok: %s=%lld\r\n", kw, val);
            }
            return;
        }
    }
    printf("err: unknown command '%s' (try 'help')\r\n", kw);
}

static char serial_buf[64];
static int  serial_len = 0;

void serial_poll(void) {
    int c;
    while ((c = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT) {
        if (c == '\r' || c == '\n') {
            if (serial_len > 0) {
                serial_buf[serial_len] = '\0';
                serial_handle(serial_buf);
                serial_len = 0;
            }
        } else if (c == '\b' || c == 127) {
            if (serial_len > 0) { serial_len--; printf("\b \b"); }
        } else if (serial_len < (int)sizeof(serial_buf) - 1) {
            serial_buf[serial_len++] = (char)c;
            putchar(c);
        }
    }
}
