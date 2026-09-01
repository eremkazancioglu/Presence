# Bedside Focus Badge

A BLE button worn on a hospital staff badge that toggles focus/do-not-disturb
mode on the wearer's phone (and, by Apple's own sync, their Watch). See
[CLAUDE.md](CLAUDE.md) for full project background and scope.

**Status:** phase 1 prototype — manual button press only. Badge firmware
implements a BLE HID keyboard design (see "What it does" below); compiles
clean, not yet tested end-to-end on real hardware. No battery, no
enclosure yet.

## Repo layout

- `firmware/badge/` — Arduino sketch for the XIAO ESP32C6.
- `scripts/` — Python tooling (BLE scanner, mainly useful pre-pairing —
  see `scripts/scan.py`'s docstring).
- `ios/` — SwiftUI companion app. No longer the trigger mechanism (see
  `docs/ble-protocol.md` for why) — kept as a working foreground
  debug/status tool. See [ios/README.md](ios/README.md).
- `docs/` — protocol/design notes.

## Hardware

- Seeed Studio XIAO ESP32C6, USB-C powered.
- Tactile push button (not yet wired — see "Temporary input" below).

## Firmware

### Setup (Arduino IDE)

1. Install the **esp32** board package (Espressif Systems) via
   Boards Manager.
2. Install the **NimBLE-Arduino** library (by h2zero) via Library Manager.
3. Install the **HijelHID_BLEKeyboard** library (by Hijel) via Library
   Manager.
4. Select board **XIAO_ESP32C6**, and the correct USB serial port.
5. Open `firmware/badge/badge.ino` and upload.
6. Open the Serial Monitor at 115200 baud to see debug output.

Can also be compiled from the command line via `arduino-cli` (already
configured against the same board/library install as the Arduino IDE on
this machine):

```sh
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C6 firmware/badge/badge.ino
```

### Temporary input

The tactile button hasn't arrived yet, so the firmware currently reads
header pin **D0**. Simulate a press with a single jumper wire: touch one
end to the pin labeled **D0** and the other to a **GND** pin, then remove
it — that's one press. Once the real button is in hand, wire it between a
GPIO and GND and update `BUTTON_PIN` in `badge.ino`; the debounce/toggle
logic doesn't need to change.

Note: on the XIAO ESP32C6, silkscreen pin labels (`D0`, `D9`, ...) don't
map 1:1 to GPIO numbers in code (e.g. `D9` is actually GPIO20, not GPIO9)
— always use the `D#` macros in code rather than raw GPIO numbers, so the
pin referenced in code matches the pin printed on the board.

### What it does

The badge is a BLE HID keyboard (device name `PresenceBadge`), pairs once
via Settings → Bluetooth, then stays continuously connected — like a real
Bluetooth keyboard. Each button press sends one keystroke: **F13** for
"on", **F14** for "off". On the phone, a native OS-level key binding
(Settings → Accessibility → Full Keyboard Access → Commands) runs the
corresponding Set Focus shortcut directly — no app, no automation, no
Bluetooth connect/disconnect toggling. See
[docs/ble-protocol.md](docs/ble-protocol.md) for the full design and why
(this supersedes two earlier designs — broadcast/iBeacon, then bonded
connect/disconnect toggling — that didn't hold up for background/locked
reliability).

## Pairing and setting up the key bindings

1. Flash the badge, press it once so it starts advertising for pairing.
2. On the phone: Settings → Bluetooth → tap **PresenceBadge** under Other
   Devices to pair.
3. Settings → Accessibility → Keyboards & Typing → Full Keyboard Access →
   turn it on → **Commands** → find/add a command bound to key **F13** →
   set it to run the **Badge Focus On** shortcut (see
   [ios/PresenceBadge/Resources/Shortcuts/README.md](ios/PresenceBadge/Resources/Shortcuts/README.md)
   if that shortcut doesn't exist yet).
4. Repeat for **F14** → **Badge Focus Off**.
5. Turn on **Allow Running While Locked** for both shortcuts (Shortcuts
   app → shortcut → (i) info icon).

## Verifying the badge's advertising with the scanner script

Before pairing, confirm the badge broadcasts when expected:

```sh
uv sync
uv run scripts/scan.py
```

Power the badge — you should see "Badge advertising (wants to connect)"
appear before it's paired with anything.

## Not built yet

Battery/power management, enclosure, multi-device pairing, proximity-based
(phase 2) triggering, real end-to-end validation of the HID-keyboard
design against actual hardware.
