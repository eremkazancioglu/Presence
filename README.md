# Bedside Focus Badge

A BLE button worn on a hospital staff badge that toggles focus/do-not-disturb
mode on the wearer's phone (and, by Apple's own sync, their Watch). See
[CLAUDE.md](CLAUDE.md) for full project background and scope.

**Status:** phase 1 prototype — manual button press only. Badge firmware
implements a bonded, button-driven BLE connect/disconnect design (see
"What it does" below); not yet tested end-to-end with a real Shortcuts
automation. No battery, no enclosure yet.

## Repo layout

- `firmware/badge/` — Arduino sketch for the XIAO ESP32C6.
- `scripts/` — Python tooling (BLE scanner for verifying the badge's
  advertising behavior).
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
3. Select board **XIAO_ESP32C6**, and the correct USB serial port.
4. Open `firmware/badge/badge.ino` and upload.
5. Open the Serial Monitor at 115200 baud to see debug output.

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

The badge bonds with the phone once (via Settings → Bluetooth), then the
button drives connect/disconnect: pressing "on" makes the badge
connectable, which the phone auto-reconnects to (a bonded/trusted
device); pressing "off" makes the badge actively disconnect. A native
Shortcuts personal automation reacts to those connect/disconnect events
to toggle Focus mode — see [docs/ble-protocol.md](docs/ble-protocol.md)
for the full design and why (this supersedes an earlier broadcast/iBeacon
approach that didn't work reliably in the background).

## Pairing and setting up the automations

1. Flash the badge, press it once ("on") so it's advertising.
2. On the phone: Settings → Bluetooth → tap **PresenceBadge** under Other
   Devices to pair.
3. In the Shortcuts app: Automation → **+** → **Bluetooth** → select
   **PresenceBadge** → **Connects** → Run immediately → add action
   **Run Shortcut** → choose **Badge Focus On** (see
   [ios/PresenceBadge/Resources/Shortcuts/README.md](ios/PresenceBadge/Resources/Shortcuts/README.md)
   if that shortcut doesn't exist yet).
4. Repeat for **Disconnects** → **Badge Focus Off**.

## Verifying the badge's advertising with the scanner script

Before involving the phone at all, confirm the badge broadcasts when
expected:

```sh
uv sync
uv run scripts/scan.py
```

Jumper D0 to GND (see "Temporary input" above) — you should see "Badge
advertising (wants to connect)" appear. Press again — since it's not
bonded to this laptop, it'll just keep advertising (a real phone would
connect and it'd disappear from the scan).

## Not built yet

Battery/power management, enclosure, GATT service, multi-device pairing,
proximity-based (phase 2) triggering, real end-to-end validation of the
Bluetooth-automation design against actual hardware.
