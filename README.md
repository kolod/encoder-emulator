# Encoder Emulator

Quadrature encoder emulator for the [Waveshare RP2040-Zero](https://www.waveshare.com/wiki/RP2040-Zero).  
Outputs Phase A / Phase B / Index signals with a configurable trapezoidal motion profile.  
A 1.3" OLED display with a rotary encoder and two buttons provides a live UI; all parameters are also accessible over USB serial.

Licensed under the GNU General Public License v3 or later.

---

## Hardware

| Function | Pin |
|---|---|
| OLED SDA (I2C0) | GP0 |
| OLED SCL (I2C0) | GP1 |
| Button BACK | GP3 |
| Button CONFIRM | GP4 |
| Display encoder PUSH | GP5 |
| Display encoder Phase A | GP6 |
| Display encoder Phase B | GP7 |
| Emulated encoder Phase A | GP8 |
| Emulated encoder Phase B | GP9 |
| Emulated encoder Index | GP10 |
| WS2812 LED | GP16 |

The display board is a 1.3" SSD1306 OLED (128 × 64, I2C address 0x3C) with an integrated rotary encoder and two buttons.

---

## Architecture

The firmware runs on both RP2040 cores simultaneously:

- **Core 0** — drives the emulated encoder outputs (GP8/9/10) using a trapezoidal motion profile.  Each step sleeps for exactly `1 000 000 / speed` µs, so the output frequency tracks the target speed precisely.
- **Core 1** — runs the display, reads UI inputs, and processes USB serial commands.  The display refreshes every 50 ms; inputs are read every 10 ms.

A mutex protects all shared motion state between the two cores.

**PIO state machines:**
- `ws2812` (pio0) — drives the onboard WS2812 LED.
- `input_debounce` (pio1) — monitors all five UI input pins simultaneously.  Each stable state change (after a configurable settle window, default 5 ms) is pushed to the FIFO; the CPU decodes button edges and quadrature direction from consecutive snapshots.

---

## Building

Prerequisites: [Raspberry Pi Pico SDK 2.2.0](https://github.com/raspberrypi/pico-sdk), CMake ≥ 3.20, arm-none-eabi toolchain 15.2.

```sh
mkdir build && cd build
cmake ..
make -j
```

The `u8g2` display library is fetched automatically from GitHub during the first configure.

After a successful build, `arm-none-eabi-size` prints the flash and RAM usage.

---

## Flashing

**Automatic (runs after every build):**

Python 3 and the `pyserial` + `rich` packages are required.

```sh
pip install pyserial rich
```

The `flash.py` script is invoked automatically by CMake after each build.  It:
1. Tries to find a USB serial port with the RP2040 VID (0x2E8A) and sends the `boot` command to reboot into BOOTSEL mode.
2. Waits for the RPI-RP2 mass-storage drive to appear.
3. Copies the UF2 file to the drive.

If no device is found, the script exits silently so the build does not fail.

To specify the serial port manually:

```sh
cmake -DFLASH_PORT=COM3 ..
```

**Manual:**

Hold the BOOTSEL button while plugging in the USB cable, then copy `build/encoder-emulator.uf2` to the `RPI-RP2` drive.

---

## Display UI

### Root screen

Shows live encoder state:

```
Current pos:
<value>
Target pos:
<value>
Speed:
<value>
```

Rotate the display encoder to increment / decrement the target position by the configured **Incr** step.  
Press **CONFIRM** to open the menu.

### Menu

```
=== Menu ===
> Reset position
  Parameters
```

- **Reset position** — sets target and current position to 0 immediately.
- **Parameters** — opens the parameter editor.

Press **BACK** to return to the root screen.

### Parameters

| Label | Description | Default | Range |
|---|---|---|---|
| Tgt pos | Target position (steps) | 0 | ±2 000 000 000 |
| Tgt spd | Target speed (steps/s) | 1 000 | 1 – 100 000 |
| Accel | Acceleration (steps/s²) | 100 | 1 – 100 000 |
| Decel | Deceleration (steps/s²) | 100 | 1 – 100 000 |
| PPR | Pulses per revolution | 1 000 | 1 – 32 767 |
| Incr | Position increment per encoder click | 1 | 1 – 1 000 000 |
| Linear | 0 = rotary, 1 = linear (index at position 0 only) | 0 | 0 – 1 |

Rotate the encoder to move the `>` cursor.  Press **CONFIRM** to start editing the selected parameter (value shown in `[brackets]`).  Rotate to change the value; press **CONFIRM** or **BACK** to confirm.  The **Linear** toggle changes immediately on **CONFIRM** with no edit mode.

---

## USB Serial Interface

Connect at any baud rate (USB CDC).  Commands are newline-terminated.

| Command | Description |
|---|---|
| `pos <n>` | Set target position (steps) |
| `spd <n>` | Set target speed (steps/s) |
| `accel <n>` | Set acceleration (steps/s²) |
| `decel <n>` | Set deceleration (steps/s²) |
| `ppr <n>` | Set pulses per revolution |
| `reset` | Zero target and current position immediately |
| `get` | Print current position, speed, and all parameters |
| `boot` | Reboot into BOOTSEL (programming) mode |
| `help` / `?` | Print command list |

---

## Tuning

**Debounce settle time** (`DEBOUNCE_SETTLE_US` in `board.h`, default 5000 µs):  
All five UI inputs share a single PIO debounce filter.  Increase if inputs are still noisy; decrease if the encoder feels sluggish at high rotation speeds.  The maximum responsive rotation rate is approximately `1 000 000 / (4 × DEBOUNCE_SETTLE_US)` detents per second.

**Motion profile:**  
The trapezoidal ramp uses kinematic equations (v² = u² ± 2a) computed per output step, so the speed and acceleration parameters are in true steps/s and steps/s².  The deceleration look-ahead ensures the motor always stops exactly on the target position without overshoot.
