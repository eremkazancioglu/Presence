# Bedside Focus Badge

A Bluetooth button worn on a hospital staff badge that toggles
focus/do-not-disturb mode on the wearer's phone (and, by Apple's own
sync, their Watch). See [CLAUDE.md](CLAUDE.md) for full project
background and scope.

**Status:** phase 1 prototype — manual button press only, verified
working end-to-end on real hardware, including with the phone locked.
Seven trigger-mechanism designs were tried before landing on the current
one — see [docs/trigger-mechanism-investigation.md](docs/trigger-mechanism-investigation.md)
for the full story. No battery, no enclosure yet.

## Repo layout

- `firmware/experiments/classic_bt_hid_switch/` — **current, recommended
  firmware.** ESP-IDF project (not Arduino) for a plain ESP32-WROOM-32.
- `firmware/badge/` — earlier, superseded firmware (still works). Arduino
  sketch for the XIAO ESP32C6.
- `scripts/` — Python tooling (BLE scanner, relevant to the superseded
  design only — see `scripts/scan.py`'s docstring).
- `ios/` — SwiftUI companion app. Not the trigger mechanism for either
  design (see `docs/ble-protocol.md` for why) — kept as a working
  foreground debug/status tool. See [ios/README.md](ios/README.md).
- `docs/` — protocol/design notes.

## Hardware

- **Current:** plain ESP32-WROOM-32 DevKit (e.g. AITRIP, CP2102
  USB-UART bridge, 30-pin), USB-C powered. Needed specifically because
  this design requires Classic Bluetooth (BR/EDR); the XIAO ESP32C6
  below doesn't have that radio.
- **Superseded:** Seeed Studio XIAO ESP32C6, USB-C powered — used by
  `firmware/badge/`.
- Tactile push button (not yet wired on either board — see each
  firmware's setup steps for the temporary jumper-wire substitute).

## Current firmware: Classic Bluetooth HID switch

`firmware/experiments/classic_bt_hid_switch/` — the badge presents as a
Classic Bluetooth (BR/EDR) Keyboard-class HID device. Button-driven:
first press goes discoverable for one-time pairing; each later press
actively connects or disconnects depending on current state; after a
disconnect it stays off until the next press (no auto-reconnect). A
native Shortcuts "Bluetooth device connects/disconnects" personal
automation reacts directly to those events — no app, no Full Keyboard
Access, no keystrokes ever sent. Full design rationale (including why
Keyboard class specifically, and why this needed a different chip) in
[docs/trigger-mechanism-investigation.md](docs/trigger-mechanism-investigation.md#7-classic-bluetooth-bredr-hid-device--connectdisconnect).

### Setup (ESP-IDF, not Arduino)

The Classic BT HID Device API this firmware uses isn't exposed by the
Arduino-ESP32 core at all, so this project needs the real ESP-IDF
toolchain:

```sh
mkdir -p ~/esp && cd ~/esp
git clone --recursive -b release/v5.3 https://github.com/espressif/esp-idf.git
cd esp-idf && ./install.sh esp32
```

Also needs `cmake` and `ninja` (`brew install cmake ninja` on macOS —
ESP-IDF doesn't bundle these itself there), and the Silicon Labs CP210x
VCP driver for the AITRIP board's USB-UART chip (download from
[silabs.com](https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers),
install, then approve it under System Settings → Privacy & Security, and
restart).

To build and flash:

```sh
source ~/esp/esp-idf/export.sh   # run in every fresh terminal, before idf.py
cd firmware/experiments/classic_bt_hid_switch
idf.py set-target esp32
idf.py -p /dev/cu.<yourport> flash monitor   # find the port via `ls /dev/cu.*`
```

If `idf.py` fails with a Python virtualenv error, your shell's default
`python3` doesn't match the one ESP-IDF was installed against — prefix
with `export PATH="/opt/homebrew/bin:$PATH"` first (or whichever `python3`
you originally installed ESP-IDF under).

### Temporary input

No tactile button wired yet — jumper a wire from pin **D4** to any
**GND** pin to simulate a press (unlike the XIAO board, this DevKit's
`D#` silkscreen labels are the real GPIO numbers, no remapping quirk).

### Pairing and automation setup

1. Flash the badge, press the button once (jumper D4 to GND) so it goes
   discoverable.
2. On the phone: Settings → Bluetooth → tap **PresenceBadge** under
   Other Devices to pair.
3. Shortcuts app → Automation → **+** → New Personal Automation →
   Bluetooth → select **PresenceBadge**, enable both **Connects**
   and **Disconnects** (as two separate automations) → add action **Run
   Shortcut** → pick **Badge Focus On** (for Connects) or **Badge Focus
   Off** (for Disconnects) → turn off **Ask Before Running** on each.
4. Turn on **Allow Running While Locked** for both `Badge Focus On`/`Off`
   shortcuts (Shortcuts app → shortcut → (i) info icon) if not already on.

## Superseded firmware: BLE HID keyboard + Full Keyboard Access

`firmware/badge/` — still works, still verified on real hardware
including locked, but not the recommended path: Full Keyboard Access
shows a persistent on-screen highlight box the entire time it's enabled,
not just while actively navigating, which is real ongoing friction for
distributing this to many users. Kept as a working fallback.

### Setup (Arduino IDE)

1. Install the **esp32** board package (Espressif Systems) via
   Boards Manager.
2. Install the **NimBLE-Arduino** library (by h2zero) via Library Manager.
3. Install the **HijelHID_BLEKeyboard** library (by Hijel) via Library
   Manager.
4. Select board **XIAO_ESP32C6**, and the correct USB serial port.
5. Open `firmware/badge/badge.ino` and upload.
6. Open the Serial Monitor at 115200 baud to see debug output.

Can also be compiled from the command line via `arduino-cli`:

```sh
arduino-cli compile --fqbn esp32:esp32:XIAO_ESP32C6 firmware/badge/badge.ino
```

### Temporary input

Reads header pin **D0** — jumper it to a **GND** pin to simulate a press.
Note: on the XIAO ESP32C6, silkscreen pin labels (`D0`, `D9`, ...) don't
map 1:1 to GPIO numbers in code (e.g. `D9` is actually GPIO20, not GPIO9)
— always use the `D#` macros in code rather than raw GPIO numbers.

### What it does

The badge is a BLE HID keyboard (device name `PresenceBadge`), pairs once
via Settings → Bluetooth, then stays continuously connected. Each button
press sends **Ctrl+Option+O** ("on") or **Ctrl+Option+F** ("off"),
repeated redundantly since single-attempt delivery isn't reliable while
the phone is locked. On the phone, Settings → Accessibility → Full
Keyboard Access → Commands binds each key directly to a Shortcut. Full
detail in [docs/ble-protocol.md](docs/ble-protocol.md).

## Verifying advertising with the scanner script (superseded design only)

```sh
uv sync
uv run scripts/scan.py
```

Power the badge — you should see "Badge advertising (wants to connect)"
appear before it's paired with anything. Not relevant to the current
Classic BT design, which has no advertising payload to scan for.

## Not built yet

Battery/power management, enclosure, multi-device pairing, proximity-based
(phase 2) triggering.
