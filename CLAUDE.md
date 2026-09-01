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
- Firmware has gone through three designs (broadcast/iBeacon -> bonded
  connect/disconnect -> current: BLE HID keyboard) chasing reliable
  background/locked-phone behavior — full history in Architecture
  decisions below. **Current firmware (BLE HID keyboard, F13/F14
  keystrokes) compiles clean via `arduino-cli` but is not yet tested
  end-to-end on real hardware** — that's the immediate next step, not
  writing more code.
- **Gotcha:** on the XIAO ESP32C6, silkscreen pin labels don't map 1:1 to
  raw GPIO numbers in Arduino code (e.g. label `D9` is actually GPIO20,
  not GPIO9). Always reference pins via the `D#` macros in code, not raw
  GPIO numbers, so the pin in code matches the pin printed on the board.
- iOS companion app (`ios/`, SwiftUI + xcodegen) builds and runs on real
  hardware; its own foreground behavior (badge -> app -> Set Focus
  shortcut -> Do Not Disturb) was verified working under the first
  (now-superseded) iBeacon design. No longer the trigger mechanism — see
  Architecture decisions below — kept as a foreground debug tool.
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

- **Badge is a BLE HID keyboard; the trigger is a native OS-level key
  binding, not a Shortcuts automation or an app.** (Third and current
  design — supersedes both "broadcast, not pair" and "bonded connect/
  disconnect" below.) Built with the `HijelHID_BLEKeyboard` Arduino
  library. Pairs once via Settings → Bluetooth, then stays continuously
  connected like a real Bluetooth keyboard (no per-press
  connect/disconnect toggling). Each press sends a distinct keystroke —
  F13 for "on", F14 for "off" (otherwise-unused function keys, won't
  collide with typing). On the phone: Settings → Accessibility →
  Keyboards & Typing → Full Keyboard Access → Commands binds each key
  directly to a Shortcut. Confirmed empirically (with a real Bluetooth
  accessory, before committing to this design) that this fires reliably
  even while the phone is locked, and — critically, unlike the two
  earlier designs — iOS auto-reconnects HID-classified devices at the
  system level without any app running, which is documented behavior
  (unlike for generic BLE peripherals, see below), so the connection
  itself should stay reliable too.
  - *Second design, superseded:* bonded BLE connect/disconnect, reacted
    to by a native Shortcuts "When Bluetooth device connects/disconnects"
    automation. The automation-fires-while-locked part worked (verified
    empirically), but two problems killed it: (1) a plain custom BLE
    peripheral doesn't appear in Settings → Bluetooth at all, so there's
    nothing to pair — worked around by masquerading as a Heart Rate
    device (an "adopted" GATT service iOS does list), since the properly-
    sanctioned fix, Apple's `AccessorySetupKit`, needs an entitlement
    Apple has to approve and isn't available for this prototype; and (2)
    even once paired that way, iOS doesn't auto-reconnect a generic BLE
    peripheral without an app actively driving it — only specific
    recognized classes (HID, audio devices) get app-free auto-reconnect,
    which is exactly why the current design uses real HID instead of
    continuing to fight this.
  - *Original design, superseded:* continuous BLE broadcast on button
    press (custom UUID/payload), no pairing at all — any number of
    listening devices react to the same broadcast independently,
    phone-side automation reacts to the advertisement. Simpler and worked
    fine in the foreground, but see the background-reliability
    investigation below for why it didn't hold up for the badge's actual
    purpose.
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
  direct API call.** No public iOS API lets a third-party app, automation,
  or key binding toggle Focus mode directly — only Shortcuts' own "Set
  Focus" action can. The two shortcuts (`Badge Focus On` / `Badge Focus
  Off`, each just a Set Focus action targeting Do Not Disturb) are
  hand-built once in the Shortcuts app — `.shortcut` files are a signed
  binary format that can't be generated programmatically. **Both need
  "Allow Running While Locked" turned on** (per-shortcut setting, in the
  shortcut's own (i) info screen) for the F13/F14 key bindings to work
  while the phone is locked.
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
  indefinitely~~ — revisited three times, landed on neither: the final
  answer is a native OS-level key binding (Full Keyboard Access), not an
  app, a third-party Shortcuts trigger app, or even a Shortcuts
  automation. See Architecture decisions above for the full path there.
  The custom app remains in the repo as a working foreground
  tool/foundation, not abandoned, just not load-bearing for phase 1.
- Per-badge unique identity — not needed while only one badge exists.
  Under the current HID design this would mean a distinct
  name/keystroke-pair per badge, and each wearer's Full Keyboard Access
  bindings pointing at their own badge's keys specifically. Needs real
  design once more than one badge is in use.
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