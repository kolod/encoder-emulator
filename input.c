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
#include "hardware/irq.h"
#include "board.h"
#include "input.h"

// ---- Ring buffer -----------------------------------------------------------
// The PIO RX interrupt drains the PIO FIFO into this buffer immediately,
// so no state transitions are lost even when core1 is blocked in draw_screen.

#define RINGBUF_SIZE 256u  // must be a power of 2

static uint32_t      ringbuf[RINGBUF_SIZE];
static volatile uint ringbuf_head = 0;  // written by ISR only
static volatile uint ringbuf_tail = 0;  // written by main loop only

static debounce_t *g_debounce;

static void debounce_rx_irq(void) {
    debounce_t *d = g_debounce;
    while (!pio_sm_is_rx_fifo_empty(d->pio, d->sm)) {
        uint32_t v    = pio_sm_get(d->pio, d->sm);
        uint     next = (ringbuf_head + 1u) & (RINGBUF_SIZE - 1u);
        if (next != ringbuf_tail) {   // drop only if ring buffer itself is full
            ringbuf[ringbuf_head] = v;
            ringbuf_head          = next;
        }
    }
}

void input_irq_init(debounce_t *d) {
    g_debounce = d;
    uint irq_num = (d->pio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;
    pio_set_irq0_source_enabled(d->pio,
        (pio_interrupt_source_t)(pis_sm0_rx_fifo_not_empty + d->sm), true);
    irq_set_exclusive_handler(irq_num, debounce_rx_irq);
    irq_set_enabled(irq_num, true);
}

// ---- Event decoding --------------------------------------------------------

// Quadrature direction table.
// Index = (prev_ab << 2) | curr_ab, where ab = (state >> 3) & 0x3:
//   bit 0 = ENC_A (IN base + 3), bit 1 = ENC_B (IN base + 4).
static const int8_t qdir[16] = {
     0, +1, -1,  0,   // prev AB = 00
    -1,  0,  0, +1,   // prev AB = 01
    +1,  0,  0, -1,   // prev AB = 10
     0, -1, +1,  0,   // prev AB = 11
};

// Per-detent accumulator and direction-change holdoff state.
static int             enc_acc;
static absolute_time_t enc_holdoff_until;
static bool            enc_last_plus;

static event_t decode(uint32_t prev, uint32_t next) {
    // Buttons are active-low; detect falling edge (1 → 0 = pressed).
    uint32_t fell = prev & ~next;
    if (fell & (1u << 0)) return EVENT_BACK;
    if (fell & (1u << 1)) return EVENT_CONFIRM;
    if (fell & (1u << 2)) return EVENT_ENC_PUSH;

    // Quadrature: bits [4:3] carry ENC_B and ENC_A respectively.
    uint8_t prev_ab = (uint8_t)((prev >> 3) & 0x3);
    uint8_t next_ab = (uint8_t)((next >> 3) & 0x3);
    int8_t  dir     = qdir[(prev_ab << 2) | next_ab];
    if (dir == 0) return EVENT_NONE;

    // Accumulate raw steps; reset on direction change to avoid carry-over.
    if (enc_acc > 0 && dir < 0) enc_acc = 0;
    if (enc_acc < 0 && dir > 0) enc_acc = 0;
    enc_acc += dir;

    // Emit one event per full detent (ENC_STEPS_PER_DETENT consistent steps).
    bool is_plus;
    if      (enc_acc >=  ENC_STEPS_PER_DETENT) { enc_acc = 0; is_plus = true;  }
    else if (enc_acc <= -ENC_STEPS_PER_DETENT) { enc_acc = 0; is_plus = false; }
    else return EVENT_NONE;

    // Suppress a direction reversal within the holdoff window (snap-back filter).
    if (!time_reached(enc_holdoff_until) && is_plus != enc_last_plus)
        return EVENT_NONE;

    enc_last_plus     = is_plus;
    enc_holdoff_until = delayed_by_ms(get_absolute_time(), ENC_HOLDOFF_MS);
    return is_plus ? EVENT_ENCODER_PLUS : EVENT_ENCODER_MINUS;
}

// ---- Poll ------------------------------------------------------------------

event_t input_poll(uint32_t *prev_state) {
    while (ringbuf_tail != ringbuf_head) {
        uint32_t state = ringbuf[ringbuf_tail];
        ringbuf_tail   = (ringbuf_tail + 1u) & (RINGBUF_SIZE - 1u);
        event_t ev     = decode(*prev_state, state);
        *prev_state    = state;
        if (ev != EVENT_NONE) return ev;
    }
    return EVENT_NONE;
}
