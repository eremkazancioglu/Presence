# Bedside Focus Badge

A BLE button worn on a hospital staff badge that toggles focus/do-not-disturb
mode on the wearer's phone (and, by Apple's own sync, their Watch). See
[CLAUDE.md](CLAUDE.md) for full project background and scope.

**Status:** phase 1 prototype — manual button press only. Badge firmware
and laptop verification tool work end-to-end; iOS companion app builds and
runs on real hardware, foreground behavior verified, background/locked-
phone behavior not yet validated. No battery, no enclosure yet.

## Repo layout

- `firmware/badge/` — Arduino sketch for the XIAO ESP32C6.
- `scripts/` — Python tooling (BLE scanner for verifying the badge works).
- `ios/` — SwiftUI companion app that reacts to the badge and toggles
  Focus mode. See [ios/README.md](ios/README.md).
- `docs/` — protocol/design notes.

## Hardware

- Seeed Studio XIAO ESP32C6, USB-C powered.
- Tactile push button (not yet wired — see "Temporary input" below).

## Firmware

### Setup (Arduino IDE)

1. Install the **esp32** board package (Espressif Systems) via
   Boards Manager.
2. Install the **NimBLE-Arduino** library (by h2zero) via Library Manager.
3. Select board **XIAO_ESP32C6**, and the correct USB serial port.
4. Open `firmware/badge/badge.ino` and upload.
5. Open the Serial Monitor at 115200 baud to see debug output.

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

On each button press, the badge toggles an internal on/off state and
updates a continuous BLE advertisement reflecting it, in standard iBeacon
format (UUID + Major + Minor) so the iOS app can react to it reliably even
while backgrounded. No pairing, no GATT service. See
[docs/ble-protocol.md](docs/ble-protocol.md) for the exact payload format
and why iBeacon specifically.

## Verifying it with the scanner script

With the badge powered and advertising, run the scanner from a laptop with
Bluetooth:

```sh
uv sync
uv run scripts/scan.py
```

Jumper D0 to GND on the badge (see "Temporary input" above) — you should
see `Focus ON` / `Focus OFF` printed as the state toggles.

## Not built yet

Battery/power management, enclosure, GATT service, multi-device pairing,
proximity-based (phase 2) triggering. iOS app is scaffolded (see
[ios/README.md](ios/README.md)) but not yet validated against real
hardware end-to-end.
