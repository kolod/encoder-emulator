#!/usr/bin/env python3
"""
Flash RP2040 firmware after a successful build.

Steps:
  1. If the RPI-RP2 mass-storage drive is already visible, copy directly.
  2. Otherwise find the CDC serial port, send 'boot', then wait for the drive.
  3. Copy the UF2 to the drive.

Usage:
  flash.py [--port <COM3>] [--verbose] <firmware.uf2>

If no device is reachable at all, exit 0 so the build is not marked failed.
Requires: pyserial  (pip install pyserial)
          rich      (pip install rich)
"""

import sys
import os
import time
import shutil
import string
import argparse

from rich.console import Console
from rich.status  import Status

VOLUME_LABEL = "RPI-RP2"
RP2040_VID   = 0x2E8A
BOOT_TIMEOUT = 10   # seconds to wait for the drive after sending boot command

console = Console(highlight=False)
_verbose = False


def vprint(msg: str) -> None:
    if _verbose:
        console.print(f"  [dim]{msg}[/dim]")


# ---- Drive detection --------------------------------------------------------

def find_rpi_drive() -> str | None:
    if sys.platform == "win32":
        import ctypes
        vol  = ctypes.create_unicode_buffer(256)
        mask = ctypes.windll.kernel32.GetLogicalDrives()
        for i, ch in enumerate(string.ascii_uppercase):
            if not (mask & (1 << i)):
                continue
            drive = f"{ch}:\\"
            ctypes.windll.kernel32.GetVolumeInformationW(
                drive, vol, 256, None, None, None, None, 0)
            vprint(f"Drive {drive}: label='{vol.value}'")
            if vol.value == VOLUME_LABEL:
                return drive
    elif sys.platform == "darwin":
        p = f"/Volumes/{VOLUME_LABEL}"
        vprint(f"Checking {p}")
        if os.path.isdir(p):
            return p
    else:
        for base in (f"/media/{os.environ.get('USER', '')}", "/media", "/mnt"):
            p = os.path.join(base, VOLUME_LABEL)
            vprint(f"Checking {p}")
            if os.path.isdir(p):
                return p
    return None


# ---- Serial port ------------------------------------------------------------

def list_serial_ports():
    try:
        import serial.tools.list_ports
        return list(serial.tools.list_ports.comports())
    except ImportError:
        return []


def auto_find_port() -> str | None:
    ports = list_serial_ports()
    for p in ports:
        vid_str = f"{p.vid:#06x}" if p.vid else "unknown"
        vprint(f"Port {p.device}: VID={vid_str} desc='{p.description}'")
        if p.vid == RP2040_VID:
            return p.device
    return None


def send_boot(port_name: str) -> bool:
    try:
        import serial
    except ImportError:
        console.print("  [yellow]pyserial not installed — cannot send boot command[/yellow]")
        console.print("  [dim]Install with:  pip install pyserial[/dim]")
        return False
    try:
        vprint(f"Opening {port_name} ...")
        with serial.Serial(port_name, timeout=2) as s:
            s.write(b"boot\r\n")
            time.sleep(0.3)
        console.print(f"  Sent [bold]boot[/bold] to [cyan]{port_name}[/cyan]")
        return True
    except Exception as e:
        console.print(f"  [red]Serial error on {port_name}: {e}[/red]")
        return False


def wait_for_drive(timeout: int = BOOT_TIMEOUT) -> str | None:
    deadline = time.monotonic() + timeout
    with Status(f"  Waiting for [cyan]{VOLUME_LABEL}[/cyan] drive (up to {timeout}s) ...",
                console=console, spinner="dots"):
        while time.monotonic() < deadline:
            drive = find_rpi_drive()
            if drive:
                return drive
            time.sleep(0.25)
    return None


# ---- Main -------------------------------------------------------------------

def main() -> None:
    global _verbose

    parser = argparse.ArgumentParser(description="Flash RP2040 UF2 firmware")
    parser.add_argument("uf2", help="Path to the .uf2 file")
    parser.add_argument("-p", "--port", metavar="PORT",
                        help="Serial port to use (e.g. COM3). Auto-detected by VID if omitted.")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Print diagnostic messages (port scan, drive enumeration, etc.)")
    args = parser.parse_args()

    _verbose = args.verbose

    if not os.path.isfile(args.uf2):
        console.print(f"[bold red]ERROR:[/bold red] UF2 file not found: {args.uf2}")
        sys.exit(1)

    console.print(f"\n[bold]Flashing[/bold] [cyan]{os.path.basename(args.uf2)}[/cyan] ...")

    drive = find_rpi_drive()

    if not drive:
        port = args.port

        if port:
            vprint(f"Using explicitly specified port {port}")
        else:
            port = auto_find_port()
            if port:
                vprint(f"Auto-detected RP2040 port: {port}")

        if not port:
            ports = list_serial_ports()
            if ports:
                names = ", ".join(f"[cyan]{p.device}[/cyan]" for p in ports)
                console.print(f"  [yellow]No RP2040 port found by VID.[/yellow] Available: {names}")
                console.print(f"  [dim]Use --port <name> to specify one explicitly.[/dim]")
            else:
                console.print("  [dim]No device in BOOTSEL mode and no serial ports found — skipping flash.[/dim]")
            sys.exit(0)

        if not send_boot(port):
            console.print("  [yellow]Could not trigger boot — skipping flash.[/yellow]")
            sys.exit(0)

        drive = wait_for_drive()
        if not drive:
            console.print(f"  [yellow]Drive did not appear — skipping flash.[/yellow]")
            sys.exit(0)

    # Brief pause: Windows sometimes needs a moment after the drive letter appears
    time.sleep(0.5)

    console.print(f"  Copying to [cyan]{drive}[/cyan] ...")
    try:
        shutil.copy2(args.uf2, os.path.join(drive, os.path.basename(args.uf2)))
    except Exception as e:
        console.print(f"  [bold red]ERROR:[/bold red] Copy failed: {e}")
        sys.exit(1)

    console.print("  [bold green]Done.[/bold green]\n")


if __name__ == "__main__":
    main()
