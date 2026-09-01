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
- No firmware written yet.
- No phone-side app/automation built yet.
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

- **Broadcast, not pair.** The badge should continuously advertise a BLE
  packet on button press (custom UUID/payload), rather than maintaining a
  formal paired connection to each device. Rationale: no pairing UX
  friction, no connection-count limits, any number of listening devices can
  react to the same broadcast independently.
- **Phone does the real work.** The badge's only job is: read button state,
  broadcast a BLE signal reflecting current on/off state. All actual
  "turn on focus mode" logic lives on the phone side (a companion app, or
  for prototyping, an OS automation like iOS Shortcuts or Android
  Tasker/MacroDroid reacting to the BLE advertisement).
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
- Exact BLE payload/UUID scheme.
- Whether to build a real companion app or rely on Shortcuts/Tasker
  indefinitely for the prototype phase.
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