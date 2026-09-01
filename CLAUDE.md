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
  Tactile button itself hasn't arrived yet.
- Firmware written and verified working end-to-end: button press (via a
  temporary D0-to-GND jumper, standing in for the tactile button) toggles
  state and broadcasts it over BLE; confirmed received by
  `scripts/scan.py` on a laptop. See `firmware/badge/badge.ino` and
  `docs/ble-protocol.md`.
- **Gotcha:** on the XIAO ESP32C6, silkscreen pin labels don't map 1:1 to
  raw GPIO numbers in Arduino code (e.g. label `D9` is actually GPIO20,
  not GPIO9). Always reference pins via the `D#` macros in code, not raw
  GPIO numbers, so the pin in code matches the pin printed on the board.
- iOS companion app (`ios/`, SwiftUI + xcodegen) builds and runs on real
  hardware; foreground behavior (badge -> app -> Set Focus shortcut ->
  Do Not Disturb) verified working. Background/locked-phone behavior was
  investigated extensively (iBeacon + CoreLocation region monitoring, then
  the newer `CLMonitor` API) and neither delivered background events
  reliably in practice — see Architecture decisions below. Current plan
  has moved to a native Shortcuts Bluetooth connect/disconnect automation
  instead; **firmware needs to change to a bonded/connectable design**
  (not yet done) and the two Shortcuts need to be rebuilt around that
  trigger instead of the app's URL-scheme invocation.
- This is a personal side project, prototyping only — no hospital
  partnership or funding in place yet.

## Hardware

- **MCU:** Seeed Studio XIAO ESP32C6 (pre-soldered headers), USB-C.
  Chosen for fast shipping availability over the nRF52840 (which was the
  original preference for its lower power draw, but wasn't available with
  fast shipping). BLE 5.3 capable, which is all that's needed for phase 1.
- **Input:** single tactile momentary push button (through-hole, breadboard
  friendly).
- **Power:** USB-powered for now. No battery yet — that's a deliberate
  simplification for this phase; revisit once core logic works.
- **Prototyping:** mini breadboard + male-to-male and male-to-female jumper
  wires.

## Architecture decisions made so far

- **Pair once, then let the button drive connect/disconnect.** (Supersedes
  the original "broadcast, not pair" plan below.) The badge bonds with the
  phone once during setup. Between presses it stays non-connectable/idle
  — nothing for iOS to auto-reconnect to. Pressing "on" starts (and keeps)
  connectable advertising, so iOS's bonded auto-reconnect completes the
  connection; pressing "off" makes the badge *actively* disconnect (not
  just stop advertising — once connected it isn't advertising anyway, so
  merely stopping advertising wouldn't drop a live connection). A native
  Shortcuts personal automation ("When Bluetooth device connects/
  disconnects") drives Focus on/off from those two events. Because the
  badge stays advertising-connectable for the *entire* ON duration (not
  just at the press), an accidental mid-visit drop (RF interference, brief
  range loss) self-heals via auto-reconnect without another button press
  — shows up as a brief disconnect-then-reconnect flicker, not a silent
  stuck-off failure. This is a system-level Bluetooth connection (like
  AirPods/Watch), not one our own app manages, so it isn't subject to the
  app-background-suspension problems below.
  - *Original plan, superseded:* continuous BLE broadcast on button press
    (custom UUID/payload), no pairing at all — any number of listening
    devices react to the same broadcast independently, phone-side
    automation reacts to the advertisement. Simpler and worked fine in the
    foreground, but see the background-reliability investigation below for
    why it didn't hold up for the badge's actual purpose.
- **Why not a custom iOS app watching BLE/iBeacon advertisements
  (tried and abandoned):** built a full custom SwiftUI app using iBeacon
  advertising (UUID/Major/Minor, switched to from a custom manufacturer-data
  scheme specifically to use CoreLocation region monitoring) plus, after
  that also failed, Apple's newer `CLMonitor` API (iOS 17+) — both are
  documented as the mechanisms for reliably waking an app in the
  background/terminated state. Neither actually delivered background
  events reliably on real hardware after extensive testing (permissions,
  precise-location, Background App Refresh, Low Power Mode all verified
  not at fault; foreground detection worked correctly in both cases,
  proving the regions/conditions themselves were set up correctly).
  `CLMonitor` additionally surfaced a separate, unrelated hard blocker even
  when it did fire: a backgrounded app cannot call
  `UIApplication.open(_:)` to hand off to Shortcuts at all (`iOS` flatly
  refuses — `LSApplicationWorkspaceErrorDomain` code 115) since that's an
  intentional security boundary, not something app code can route around.
  Together this ruled out the "custom app + iBeacon" architecture for
  phase 1. The custom app (`ios/`) still exists in the repo and still
  works correctly in the foreground; it's just not the trigger mechanism
  going forward. Detailed history is in this project's conversation log if
  ever revisited.
- **iOS Focus toggling still goes through a pre-built Shortcut, not a
  direct API call.** No public iOS API lets a third-party app or an
  automation toggle Focus mode directly — only Shortcuts' own "Set Focus"
  action can. The two shortcuts (`Badge Focus On` / `Badge Focus Off`,
  each just a Set Focus action targeting Do Not Disturb) are hand-built
  once in the Shortcuts app — `.shortcut` files are a signed binary format
  that can't be generated programmatically. With the connect/disconnect
  pivot, these are now triggered by native Shortcuts personal automations
  rather than the custom app's `x-callback-url` invocation.
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

- Advertising-only vs. GATT service (advertising-only is the current
  plan for simplicity; revisit if two-way communication, e.g. battery
  status or ack, becomes necessary).
- ~~Exact BLE payload/UUID scheme~~ — decided: iBeacon format, see
  Architecture decisions above and `docs/ble-protocol.md`.
- ~~Whether to build a real companion app or rely on Shortcuts/Tasker
  indefinitely~~ — revisited twice. First decided in favor of a real
  companion app (`ios/`) using iBeacon + CoreLocation, on the theory that
  a generic Shortcuts trigger (e.g. Pushcut's iBeacon feature) couldn't
  provide precise/reliable enough background reaction. That app's own
  background mechanism then turned out not to work reliably in practice
  (see Architecture decisions above) — so the current plan is a *native*
  Shortcuts automation (Bluetooth connect/disconnect), not a third-party
  app or a custom app, for the actual trigger. The custom app remains in
  the repo as a working foreground tool/foundation, not abandoned, just
  not load-bearing for phase 1 anymore.
- Per-badge unique identity (UUID/Major) — not needed while only one badge
  exists; the RSSI proximity filter is a stand-in, but doesn't distinguish
  "my badge" from "a colleague's badge nearby," and background events have
  no RSSI at all to filter on. Needs real per-badge IDs once more than one
  badge is in use.
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