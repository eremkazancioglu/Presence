# Bedside Focus Badge — Project Brief

## Overview

A small Bluetooth button, worn on a hospital staff ID badge/lanyard, that when
pressed puts all of the wearer's paired personal devices (phone, and by
extension their Apple Watch) into focus/do-not-disturb mode. Pressed again,
it turns focus mode back off.

**Motivation:** Nursing literature (e.g. AJN's "The New Distraction at the
Bedside") documents that smartphone use during bedside care is linked to
missed status changes, errors, and reduced patient engagement. Existing
tools solve adjacent problems but not this one directly:

- **Vocera Smartbadge / Stryker Sync Badge** — hospital comms badges with a
  DND button, but it only silences *that badge*, not the wearer's personal
  phone.
- **Brick / Foqos** — NFC-tap phone focus-mode blockers, but single-device,
  consumer-focused, not badge-integrated.
- **RTLS systems** (CenTrak, Stanley Healthcare, etc.) — already deployed in
  many hospitals for location tracking; relevant infrastructure for a future
  automatic (non-manual) version of this.

**Phase 1 (this project): manual trigger.** A physical button press starts/
stops focus mode. **Phase 2 (future, not in scope yet): automatic trigger**
based on bedside proximity, likely via existing RTLS or BLE beacon
infrastructure. Do not design for phase 2 yet — keep phase 1 architecture
simple and don't over-build for anticipated future requirements.

## Current status

- Hardware ordered: Seeed Studio XIAO ESP32C6 (pre-soldered), tactile
  push buttons, mini breadboards (170-point), jumper wire kit (M-M and M-F).
  Tactile button itself hasn't arrived yet. **Second board added:** a
  plain ESP32-WROOM-32 DevKit (AITRIP, CP2102 USB-UART bridge) — needed
  because the final trigger design requires Classic Bluetooth (BR/EDR),
  which the XIAO ESP32C6 doesn't have (it's BLE-only).
- Firmware went through seven designs chasing a trigger mechanism that's
  both reliable *and* actually acceptable to use — full history in
  [docs/trigger-mechanism-investigation.md](docs/trigger-mechanism-investigation.md).
  **Current, final design: a Classic Bluetooth HID (Keyboard-class)
  device, button-driven connect/disconnect, reacted to by a native
  Shortcuts "Bluetooth device connects/disconnects" automation — no app,
  no Full Keyboard Access. Verified working end-to-end on real hardware,
  including with the phone locked.** This supersedes the earlier
  BLE-HID-keystrokes-plus-Full-Keyboard-Access design, which worked but
  had a real ongoing UX cost (a persistent on-screen highlight box) that
  makes it a poor fit for distributing to many users.
- **Gotcha (XIAO ESP32C6 only, not the WROOM-32):** silkscreen pin labels
  don't map 1:1 to raw GPIO numbers in Arduino code (e.g. label `D9` is
  actually GPIO20, not GPIO9). Always reference pins via the `D#` macros
  in code, not raw GPIO numbers, so the pin in code matches the pin
  printed on the board. The plain ESP32-WROOM-32 DevKit doesn't have this
  quirk — its `D#` silkscreen labels are the real GPIO numbers.
- iOS companion app (`ios/`, SwiftUI + xcodegen) builds and runs on real
  hardware; its own foreground behavior (badge -> app -> Set Focus
  shortcut -> Do Not Disturb) was verified working under the first
  (now-superseded) iBeacon design. No longer the trigger mechanism — see
  Architecture decisions below — kept as a foreground debug tool.
- This is a personal side project, prototyping only — no hospital
  partnership or funding in place yet.

## Hardware

- **MCU (current, for the trigger mechanism): plain ESP32-WROOM-32 DevKit**
  (AITRIP brand, CP2102 USB-UART bridge, 30-pin), USB-C. Needed because
  the final trigger design (see Architecture decisions) requires Classic
  Bluetooth (BR/EDR) HID, which only the original ESP32 silicon has —
  the C3/C6/H2/S2 family (including the XIAO ESP32C6 below) is BLE-only.
- **MCU (original, no longer used for the trigger mechanism):** Seeed
  Studio XIAO ESP32C6 (pre-soldered headers), USB-C. Chosen originally
  for fast shipping availability over the nRF52840. Its firmware
  (`firmware/badge/badge.ino`, the BLE-HID-keystrokes-plus-Full-Keyboard-Access
  design) still works and remains in the repo, but is superseded — see
  Architecture decisions.
- **Input:** single tactile momentary push button (through-hole, breadboard
  friendly). Not yet wired on either board — temporary jumper-to-GND used
  for testing (see each board's setup instructions).
- **Power:** USB-powered for now. No battery yet — that's a deliberate
  simplification for this phase; revisit once core logic works.
- **Prototyping:** mini breadboard + male-to-male and male-to-female jumper
  wires.

## Architecture decisions made so far

**Full investigation history:** seven designs were tried in sequence
before landing on one that's both reliable *and* actually acceptable to
use day to day — see
[docs/trigger-mechanism-investigation.md](docs/trigger-mechanism-investigation.md)
for the complete narrative (what was tried, why, what broke, evidence).
Condensed summary below.

- **Current, final design: a Classic Bluetooth (BR/EDR) HID device
  (Keyboard class), button-driven connect/disconnect, reacted to by a
  native Shortcuts "Bluetooth device connects/disconnects" automation —
  no app, no Full Keyboard Access, no keystrokes ever sent.** Runs on a
  plain ESP32-WROOM-32 (not the XIAO ESP32C6 — see Hardware). Boots
  non-discoverable; first press goes connectable/discoverable for
  one-time pairing via Settings → Bluetooth; each later press either
  actively connects (fires the automation's "connects" trigger) or
  actively disconnects (fires "disconnects") depending on current state;
  after a disconnect it deliberately does *not* auto-reconnect — stays
  off until the next press. The Shortcuts automation runs the same
  `Badge Focus On`/`Badge Focus Off` shortcuts as before. **Verified
  working end-to-end on real hardware, including with the phone locked.**
  - **Why Classic BT, not BLE (design 6's dead end, resolved):** design 6
    concluded the Bluetooth automation likely only recognizes Classic
    Bluetooth, based on indirect evidence, since the XIAO ESP32C6 (the
    only badge hardware at the time) is BLE-only and could never test
    this directly. That was directly confirmed later by manually adding a
    real Classic-BT accessory (a DualShock controller) to the automation
    and seeing it fire — proof the automation isn't restricted to
    audio/car devices, just to Classic Bluetooth generally. This is why a
    second board (plain ESP32, which — unlike the C3/C6/S3 family — has a
    BR/EDR radio) was introduced.
  - **Why Keyboard HID class, not Mouse:** first attempt used the same
    "Mouse" class as Espressif's own official Classic BT HID example —
    it never appeared in Settings → Bluetooth's Other Devices list at
    all. Switching to Keyboard class (matching both our own already-proven
    BLE keyboard design and the DualShock/headphone precedent) fixed it
    immediately. iOS's Classic BT pairing UI appears not to support
    generic HID mice the way it does keyboards and game controllers.
  - **Toolchain:** the Classic BT HID Device API isn't exposed by the
    Arduino-ESP32 core at all (needs a `menuconfig` option Arduino's
    precompiled build doesn't support). This firmware
    (`firmware/experiments/classic_bt_hid_switch/`) is a full ESP-IDF
    project (`idf.py build/flash`), not an Arduino sketch — the only
    firmware in this repo built that way. ESP-IDF is a separate toolchain
    from the Arduino IDE used for everything else; see the investigation
    doc for setup notes.
- **Superseded: real BLE HID keyboard, sending actual keystrokes via
  Full Keyboard Access (`firmware/badge/badge.ino`, still in the repo,
  still works).** This was the previous "final" design — genuinely
  verified working locked, via `HijelHID_BLEKeyboard`, sending
  Ctrl+Option+O/F (repeated idempotently for delivery reliability) bound
  through Settings → Accessibility → Full Keyboard Access → Commands.
  Abandoned as the recommended design (not deleted — it's a real fallback)
  because Full Keyboard Access isn't just a one-time setup step: while
  it's on, iOS permanently shows a visible highlight box around whatever
  UI element has keyboard focus, on every screen, not only while actively
  navigating — a real ongoing UX cost, not a one-time cost, and a poor
  fit for distributing to many users who'd each need to discover an
  Accessibility feature most people have never opened. Full detail
  (including the F13/F14 command-recorder bug and the repeat-delivery
  fix) in the investigation doc.
- **Other avenues tried and ruled out while searching for a
  no-Full-Keyboard-Access alternative** (full detail in the investigation
  doc): **HomeKit** (button → Home Automation → Run Shortcut) — dead on
  two counts, HomeSpan (the realistic no-MFi-certification path) only
  supports WiFi/Ethernet HAP, not BLE, and HomeKit automations require a
  Home Hub per user regardless. **WiFi network connect/disconnect
  automation** (badge as its own WiFi AP) — killed by phones only holding
  one WiFi association at a time, which would contend with hospital
  staff/guest WiFi. **An app registering itself as a custom Shortcuts
  automation trigger** — not possible; trigger types are a fixed list
  Apple defines. **CoreBluetooth background `open()` with state
  restoration** — re-tested whether calling `UIApplication.open()` from a
  CoreBluetooth (rather than CLMonitor) background callback behaves
  differently; state restoration never actually engaged, and a
  from-cold connection attempt never completed before the app was
  suspended, so this was never even conclusively tested against the
  `open()` restriction itself.
- **Why not a custom iOS app watching BLE/iBeacon advertisements (tried
  and abandoned):** built a full custom SwiftUI app (`ios/` — still in the
  repo as a foreground debug tool, not load-bearing) using iBeacon
  advertising plus CoreLocation region monitoring, then the newer
  `CLMonitor` API — both are documented as the mechanisms for reliably
  waking an app in the background/terminated state. Region monitoring
  never delivered background events reliably despite correct setup.
  `CLMonitor` did deliver them, but surfaced a separate hard blocker: a
  backgrounded app cannot call `UIApplication.open(_:)` to hand off to
  Shortcuts at all (`LSApplicationWorkspaceErrorDomain` code 115) — an
  intentional security boundary, not fixable from app code. This is the
  same restriction that later ruled out the CoreBluetooth revisit above.
  Full details in the investigation doc.
- **iOS Focus toggling still goes through a pre-built Shortcut, not a
  direct API call.** No public iOS API lets a third-party app, automation,
  or key binding toggle Focus mode directly — only Shortcuts' own "Set
  Focus" action can. The two shortcuts (`Badge Focus On` / `Badge Focus
  Off`, each just a Set Focus action targeting Do Not Disturb) are
  hand-built once in the Shortcuts app — `.shortcut` files are a signed
  binary format that can't be generated programmatically. **Both need
  "Allow Running While Locked" turned on** (per-shortcut setting, in the
  shortcut's own (i) info screen) for the key bindings to work while the
  phone is locked.
- **Do Not Disturb only for now, not a custom Focus mode.** Simpler scope;
  a custom Focus mode would need to exist identically on every user's
  phone for a bundled shortcut to work for them, and there's no API to
  create Focus modes programmatically either. Revisit if this moves beyond
  a single-wearer prototype.
- **Apple Watch needs no separate handling.** Watch mirrors iPhone Focus
  state automatically via existing Apple sync — no direct badge-to-Watch
  communication needed.
- **Other wearables (Fitbit, Garmin, etc.) are out of scope for now.** No
  general path to control third-party wearables directly; not worth the
  integration complexity until the core phone-silencing value is validated.
- **Critical alerts must NOT be silenced.** Clinical messaging platforms
  (e.g. TigerConnect) and comms badges (Vocera) deliberately let
  urgent/critical alerts break through DND. This system should follow the
  same principle: design for priority-aware filtering (route
  personal/low-priority notifications through focus mode; leave
  urgent/clinical alerts unaffected), not a blanket "silence everything"
  toggle. Use iOS Focus Filters / Android DND priority exceptions rather
  than a all-or-nothing mute.

## Open questions / not yet decided

- ~~Advertising-only vs. GATT service~~ — moot under the current HID
  design (a GATT-based HID service is required for HID itself); revisit
  if a *further* custom service (e.g. battery status/ack) becomes useful.
- ~~Exact BLE payload/UUID scheme~~ — superseded: no longer a broadcast
  payload at all under the current HID design. See `docs/ble-protocol.md`.
- ~~Whether to build a real companion app or rely on Shortcuts/Tasker
  indefinitely~~ — revisited many times, landed on neither: the final
  answer is a native Shortcuts personal automation reacting to Classic
  Bluetooth connect/disconnect events, not an app, a third-party
  Shortcuts trigger app, or a key binding. See Architecture decisions
  above for the full path there (including the Full Keyboard Access
  design that worked but was superseded). The custom app remains in the
  repo as a working foreground tool/foundation, not abandoned, just not
  load-bearing for phase 1.
- Per-badge unique identity — not needed while only one badge exists.
  Under the current Classic BT design this would mean a distinct device
  name per badge, and each wearer's Shortcuts automation pointing at
  their own badge's name specifically. Needs real design once more than
  one badge is in use.
- Android version — DND can be toggled via a direct public API
  (`NotificationManager.setInterruptionFilter` + one-time "Notification
  Policy Access" permission), no Shortcuts-style indirection needed. Real
  scope, not started.
- Enclosure/form factor — not started, comes after core logic works.
- Battery — not started, comes after core logic works.

## IP / patent notes (context, not a task for Claude Code)

Preliminary (non-legal) search turned up related-but-not-identical prior
art: Vocera's badge DND-button patents (badge-scoped, not multi-device),
a proximity-based notification-suppression patent (wearable-to-phone
distance triggers DND), and panic-button-via-BLE patents (button press ->
BLE signal -> phone action). Nothing found that covers "one badge button
press -> all paired personal devices toggle native OS focus mode
simultaneously" for a clinical use case specifically. A provisional patent
application may be filed later; keep development notes/commits dated in
case conception-date documentation is useful. Not a concern for day-to-day
coding work.

## Immediate next steps (what I need help with right now)

1. Set up a clean project repo structure (firmware/, scripts/, docs/).
2. Arduino/C++ firmware for the XIAO ESP32C6:
   - Read button GPIO state (INPUT_PULLUP, debounced).
   - On press, toggle an internal on/off state and start/stop a BLE
     advertisement reflecting that state (custom name or manufacturer
     data payload).
   - Serial print for debugging.
3. A Python test script (using `bleak`) that scans for the badge's
   advertisement and prints state changes, for use as a laptop-side
   verification tool.
4. Basic README documenting build/flash instructions.

Not needed yet: phone-side app, battery/power management, enclosure
design, BLE GATT service, multi-device pairing logic, anything related to
phase 2 (automatic detection).