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
- Firmware has gone through six designs chasing reliable
  background/locked-phone behavior — full history in
  [docs/trigger-mechanism-investigation.md](docs/trigger-mechanism-investigation.md).
  **Current design (BLE HID keyboard, Ctrl+Option+O/F keystrokes, sent
  redundantly since delivery isn't reliable on one attempt while locked)
  is verified working end-to-end on real hardware**, including with the
  phone locked.
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

**Full investigation history:** six designs were tried in sequence before
landing on one that actually works reliably in the badge's real use case
(phone locked/pocketed) — see
[docs/trigger-mechanism-investigation.md](docs/trigger-mechanism-investigation.md)
for the complete narrative (what was tried, why, what broke, evidence).
Condensed summary below.

- **Current, working design: real BLE HID keyboard, sending actual
  keystrokes, bound via a native OS-level key binding — no app, no
  automation.** Built with the `HijelHID_BLEKeyboard` Arduino library.
  Pairs once via Settings → Bluetooth, stays continuously connected like a
  real keyboard. Each press sends **Ctrl+Option+O** ("on") or
  **Ctrl+Option+F** ("off") — standard letter keys with modifiers, not
  F13/F14 (those weren't reliably distinguished by iOS's Full Keyboard
  Access command recorder — both bindings recorded identically, blocking
  the second assignment). On the phone: Settings → Accessibility →
  Keyboards & Typing → Full Keyboard Access → Commands binds each key
  directly to a Shortcut.
  - **Delivery isn't reliable on a single attempt while the phone is
    locked/sleeping** — external keyboard input received while locked
    doesn't get processed outright (it redirects to the Face ID/passcode
    screen), though testing showed it's intermittent rather than an
    absolute block (repeated taps while still on the lock screen
    eventually "take hold"). Fix: since Set Focus is idempotent,
    `toggleFocus()` resends the same target-state command up to 4 times
    (each preceded by an unbound "wake" keystroke and a real settle
    delay) instead of once — costs a few extra seconds on the first press
    after idle, acceptable given the actual use case.
  - **Considered and rejected:** making the badge stateless (one blind
    toggle keystroke) with a phone-side Shortcut reading `Get Current
    Focus` to decide which way to flip — would eliminate the (rare) risk
    of the badge's local on/off tracking drifting from the phone's actual
    state if something else changes Focus externally. Rejected because a
    toggle isn't idempotent — repeating a blind toggle for delivery
    reliability can land on the wrong final state depending on how many
    of the repeated attempts actually get through, which is
    unpredictable. Incompatible with the repeat-delivery fix above; kept
    the stateful, explicit-set-command design.
  - **Self-healing reconnects:** an involuntary BLE disconnect (RF
    interference, brief range loss) is handled entirely automatically by
    the HID library (`_onDisconnect()` calls `startAdvertising()`
    immediately) — independent of button presses, and doesn't affect the
    phone's current Focus state at all, since keystrokes are only sent in
    direct response to a press, not connection events.
- **Why not a Shortcuts automation reacting to BLE connect/disconnect
  (tried, and does work for other device types — just not this one):** a
  bonded badge toggling its own connection state, reacted to by a native
  "When Bluetooth device connects/disconnects" automation, would avoid
  needing any keystrokes at all. The automation-fires-while-locked premise
  is genuinely correct (verified with a real Bluetooth headphone
  accessory — fired reliably whether locked or unlocked). But two
  separate problems killed it for this badge specifically: (1) a plain
  custom BLE peripheral doesn't appear in Settings → Bluetooth at all
  (worked around by masquerading as a Heart Rate device, an "adopted"
  GATT service iOS does list — the properly-sanctioned fix,
  `AccessorySetupKit`, needs an Apple-approved managed entitlement not
  available for this prototype); and (2) even once paired, iOS doesn't
  auto-reconnect a generic BLE peripheral without an app running — only
  recognized classes (HID, audio) get that treatment app-free. Rebuilding
  the badge as genuine HID (to clear problem 2) solved the connection
  reliability but hit a *third*, apparently harder wall: the Shortcuts
  automation never recognized the connect/disconnect events at all, even
  with "Any Device" selected and the connection confirmed genuinely
  happening at the OS level — likely because Shortcuts' Bluetooth
  automation may only recognize Classic Bluetooth, and the XIAO ESP32C6 is
  BLE-only hardware with no Classic radio at all. Full details in the
  investigation doc.
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
  intentional security boundary, not fixable from app code. Full details
  in the investigation doc.
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