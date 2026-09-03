# Trigger mechanism investigation

How the badge actually tells the phone to toggle Focus mode went through
seven real designs before landing on one that's both reliable and
actually acceptable to use. This document walks through the full
investigation in order: what we tried, why we expected it to work, what
we found, and why it failed (or didn't). CLAUDE.md's "Architecture
decisions" section has the condensed version; this is the detailed
history, useful if this project ever needs to be revisited, ported to
Android, or re-explained from scratch.

**The core constraint that shaped everything:** the badge's whole point
is working while the phone is locked and pocketed — a nurse presses it
once at bedside and it needs to just work, not require unlocking the
phone. Every design below was ultimately evaluated against that bar, not
against "does it work in the foreground with the app open."

## 1. Custom BLE broadcast (the original plan)

**Design:** badge continuously advertises a custom manufacturer-data
payload (company ID + a state byte) reflecting on/off state. No pairing.
Any listening device — a phone automation, a laptop scanner — reads it
passively.

**Why we expected it to work:** simplest possible design, no pairing UX
friction, works for any number of listening devices independently.

**Result:** the badge's own broadcast worked correctly — verified with a
laptop `bleak` scanner (`scripts/scan.py`), which correctly printed
`Focus ON`/`Focus OFF` as the state toggled. An iOS companion app was
scaffolded to react to this broadcast, but it was never actually tested
end-to-end against this design — the architecture pivoted to iBeacon (see
below) in the very next round of changes, before the original
CoreBluetooth-based app scanner had been verified working. So "foreground
worked" is true of the firmware/broadcast itself, not of the full
badge-to-phone chain under this specific design. Background/locked-phone
reliability was the reason for the pivot regardless, and this design had
no path to it at all: raw CoreBluetooth background scanning on iOS is
throttled too heavily to be usable (roughly one discovery callback per
peripheral per background scan cycle, no delivery guarantee).

## 2. iBeacon + CoreLocation region monitoring

**Design:** switched the badge's advertisement to standard iBeacon format
(UUID + Major + Minor) specifically so a custom iOS app could use
CoreLocation's region monitoring — the API Apple documents as reliably
waking an app in the background or from a terminated state, unlike raw
CoreBluetooth scanning.

**Why we expected it to work:** it's Apple's own documented mechanism for
exactly this class of problem (background wake on a physical-world
condition).

**What we built:** a full SwiftUI companion app (`ios/`), with
`CLLocationManager` region monitoring, an onboarding flow to request
"Always" location authorization, and a Shortcuts-invocation step
(`shortcuts://x-callback-url/run-shortcut`) since no public API lets a
third-party app toggle Focus directly.

**Debugging along the way (real bugs, not platform limits):**
- `NimBLEBeacon::setManufacturerId()` (the ESP32 firmware library)
  internally byte-swaps its input — correct for Major/Minor, which need
  big-endian per the iBeacon spec, but wrong for the company ID field,
  which needs little-endian. Diagnosed by capturing the raw BLE packet
  bytes with `bleak` and manually decoding them. Without the fix, the
  badge broadcast the wrong Apple company ID and was never recognized as
  a valid iBeacon by CoreLocation at all.
- Beacon ranging/monitoring produces zero results under "Approximate"
  location (a separate per-app toggle from Always/When-In-Use
  authorization) — required requesting temporary full accuracy.
- `.onChange(of: scenePhase)` only fires on a transition, not for the
  app's initial state — since the app launches directly into `.active`,
  ranging never actually started on a fresh launch until this was fixed.

**Result:** foreground detection worked correctly once all of the above
were fixed. Background: after extensive testing (5+ minutes, confirmed
standalone with no debugger attached, confirmed Background App Refresh
and Low Power Mode weren't factors), region monitoring **never once
delivered an event while the app was genuinely suspended.** Foreground
worked, background didn't — despite being the API Apple documents for
exactly this purpose.

## 3. CLMonitor (the newer replacement API)

**Design:** `CLLocationManager`-based region monitoring is itself
soft-deprecated in newer SDKs in favor of `CLMonitor` (iOS 17+), a
different underlying subsystem for the same class of problem. Swapped to
it, keeping the same iBeacon firmware.

**Debugging along the way:**
- The Swift-facing type is `CLMonitor.BeaconIdentityCondition`, not the
  top-level `CLBeaconIdentityCondition` the ObjC header suggests —
  findable only by reverse-engineering the compiled Swift library
  (`nm`/`swift-demangle` against the SDK's `.tbd`), not from documentation.
- `CLMonitor(name:)` crashes at launch ("Monitor name is not valid") if
  the name contains dots — needed a plain alphanumeric name.

**Result — the interesting one:** `CLMonitor` genuinely *did* deliver
events while the app was backgrounded (confirmed directly: a `CLMonitor
event: ... Satisfied` log appeared, and the very next action —
attempting `UIApplication.shared.open()` to hand off to Shortcuts — is
what failed). So background *detection* was solved. But that hand-off
failed with `LSApplicationWorkspaceErrorDomain` code 115: **a backgrounded
app cannot open another app via URL scheme at all.** This is a deliberate
iOS security boundary (an arbitrary background process silently
launching another app's UI would be a serious abuse vector), not
something fixable from app code. This ruled out "custom app detects the
badge, then invokes Shortcuts" as an architecture entirely, regardless of
how reliable the detection step was.

## 4. Bonded BLE connect/disconnect + native Shortcuts automation

**Design:** stop routing through any app. Badge bonds with the phone
once, then the button toggles the BLE *connection* itself (connectable
advertising for "on," active disconnect for "off"). A **native** Shortcuts
personal automation ("When Bluetooth device connects/disconnects") reacts
directly — no app process involved at the moment of the trigger, so the
background-app restriction from design 3 doesn't apply.

**Why we expected it to work:** this is the pattern most working DIY BLE
button projects actually use, and automation-triggered shortcuts aren't
subject to the "backgrounded app can't open another app" restriction
since there's no app in the loop.

**What we hit, one wall at a time:**
- **Plain custom BLE (LE) peripherals don't appear in Settings →
  Bluetooth at all**, confirmed via Apple's own developer support
  guidance — only "adopted" GATT service types (e.g. Heart Rate) get
  listed. Worked around by having the badge advertise the standard Heart
  Rate service (0x180D) purely to get pairing UI visibility, verified
  empirically that this worked.
- The properly-sanctioned fix for BLE accessory pairing,
  **`AccessorySetupKit`** (iOS 18+), needs `com.apple.developer.accessory-setup-kit`
  — a *managed* entitlement requiring Apple's explicit approval, not
  available for a personal prototype. Ruled out before writing any code
  against it.
- Even with Settings visibility solved, **iOS doesn't auto-reconnect a
  generic BLE peripheral without an app actively running** — confirmed
  via research (not just inference) that this app-free auto-reconnect
  treatment is specific to recognized device classes (HID, audio), not
  BLE peripherals generally. Our Heart-Rate-masquerading badge got
  neither Settings visibility issues *nor* did it clear this bar, since
  Heart Rate isn't HID or audio.

This design was shelved (not because the automation-fires-while-locked
premise was wrong — a real Bluetooth headphone accessory was tested and
confirmed to fire its automation reliably whether the phone was locked or
unlocked — but because the badge itself couldn't reliably get into a
connected state for the automation to react to in the first place).

## 5. Real BLE HID keyboard, sending actual keystrokes

**Design:** if HID is the device class that gets genuine app-free
auto-reconnect, make the badge an actual HID keyboard (via the
`HijelHID_BLEKeyboard` Arduino library, chosen over hand-rolling a raw
HID report descriptor given how costly small byte-level mistakes had
already been in this investigation). Each press sends a distinct
keystroke. On the phone, **Settings → Accessibility → Full Keyboard
Access → Commands** binds each key directly to a Shortcut — a fully
native, no-app, no-automation mechanism.

**Verified before committing:** tested empirically with a real Bluetooth
mouse/accessory that this class of OS-level trigger does fire while the
phone is genuinely locked, before investing in firmware.

**Debugging along the way:**
- **F13/F14 weren't reliably distinguished by iOS's Full Keyboard Access
  command recorder** — both key-binding attempts recorded as the same
  binding, blocking the second assignment. Switched to standard letter
  keys with modifiers (Ctrl+Option+O / Ctrl+Option+F), which recorded
  correctly.
- A theory that the *first* press after BLE idle was being silently
  dropped due to report-coalescing (the press+release happening faster
  than the connection's idle-mode interval could flush) turned out to be
  wrong — extending the tap hold time had zero effect. Diagnostic logging
  showed the badge's own transmission was reliable every single time.
- **The real cause: receiving external keyboard input while the phone is
  locked doesn't get processed — it redirects to the Face ID/passcode
  screen instead.** This looked at first like a hard, unconditional
  security block. Further testing complicated that picture: with enough
  repeated taps, *while still sitting on the lock screen* (never
  actually unlocking), a command would eventually "take hold." So the
  real behavior is closer to "intermittent while locked/waking," not "flatly
  impossible" — but not reliable enough to trust on a single attempt.

## 6. HID classification + connect/disconnect (combining 4 and 5)

**Design:** get genuine HID auto-reconnect (solves design 4's dead end)
*and* avoid sending any keyboard input at all (solves design 5's
lock-screen problem) by using the same HID library purely for its
`begin()`/`end()` connection control — no keystrokes, just toggling the
BLE connection, reacted to by a native Shortcuts Bluetooth automation
(design 4's approach again, now with real HID instead of the Heart Rate
masquerade).

**Debugging along the way:** an early test showed the badge appearing to
stay "always connected" regardless of button presses — turned out to be
a red herring (resolved itself, likely a Settings UI staleness artifact
rather than a functional bug); once cleanly retested, genuine
connect/disconnect was confirmed happening at the OS level.

**Result:** with connect/disconnect confirmed genuinely working at the OS
level, **the Shortcuts Bluetooth automation still never fired — not once,
across many manual toggles, including with "Any Device" selected** rather
than the specific device. A control test in the same session (a real
Bluetooth headphone accessory) fired its automation correctly, ruling out
a general Shortcuts malfunction. The leading explanation: Shortcuts'
Bluetooth automation trigger may only recognize **Classic Bluetooth**
connections, not BLE — and the XIAO ESP32C6 is BLE-only hardware, with no
Classic Bluetooth radio at all. If true, this isn't fixable with any GATT
profile trick; it's a hardware ceiling on this chip.

## What we landed on: design 5, made reliable

Reverted to the HID-keyboard-keystroke design (5), since its failure mode
(intermittent, improving with repeated attempts) was fundamentally
different from design 6's (categorical, zero successes across many
tries) — a reliability gap is fixable; a category of device not being
recognized at all is not.

**The fix:** Set Focus is idempotent — applying "turn on" multiple times
has the same effect as once. So instead of trying to detect success,
`toggleFocus()` now sends the same target-state command up to 4 times
(each preceded by a harmless "wake" keystroke bound to nothing, plus a
real settle delay) rather than once. If any single attempt lands, the
phone ends up in the correct state. This works — confirmed on real
hardware — at the cost of a few seconds of latency on the first press
after the phone has been idle, which is an acceptable tradeoff given the
actual use case (a nurse presses once at bedside; a few seconds before
Focus engages doesn't matter).

**A related idea we considered and rejected:** making the badge
stateless (send one blind "toggle" keystroke instead of tracking on/off
locally) combined with a phone-side Shortcut that reads the actual
current Focus state (`Get Current Focus`) and flips it — this would
eliminate a real but rare failure mode (the badge's local on/off guess
drifting out of sync with the phone's actual state if something else,
like Control Center, changes Focus externally). **Rejected** because a
toggle is not idempotent — repeating a blind toggle multiple times (which
the reliability fix above requires) can land on the wrong final state
depending on how many of the repeated attempts happen to get through,
which is unpredictable. The two ideas are fundamentally incompatible;
keeping the stateful, explicit-set-command design was the correct call
given the reliability fix already works.

## The friction problem with design 5, and the search for an alternative

Design 5 (hardened) was real and it worked — verified end-to-end on real
hardware, including locked. But it has a UX cost that only became
apparent using it day to day: **Full Keyboard Access isn't just a
one-time setup step.** While it's on, iOS permanently shows a visible
highlight box around whatever UI element currently has keyboard focus —
not only while actively navigating with the keyboard, but at all times,
on every screen. Tuning FKA's Visual Options (fastest auto-hide, muted
highlight color) reduces this but can't remove it. For a badge meant to
be distributed to many hospital staff, each of whom would need to
discover and enable an Accessibility feature most people have never
opened, then live with a persistent visual side effect, this is real,
ongoing friction — not a one-time setup cost.

This prompted a broad search for any other native, hands-free trigger
mechanism, none of which panned out:

- **HomeKit** (button-press accessory → Home Automation → "Run Shortcut"
  action): dead on two independent counts. HomeSpan (the realistic
  hobbyist path to a HomeKit-compliant accessory without MFi
  certification) only implements HAP over WiFi/Ethernet, not BLE — our
  badge is BLE-only hardware. And separately, HomeKit automations
  (not just remote control) require a Home Hub (HomePod/Apple TV) to run
  at all — a non-starter for distributing to many users, each of whom
  would need their own hub.
- **Wi-Fi network connect/disconnect automation** (badge runs as a WiFi
  AP, phone auto-joins/leaves it): killed by a simple real-world fact —
  a phone can only hold one active WiFi association at a time, and a
  hospital-employed wearer's phone is very likely already joined to
  hospital staff/guest WiFi, which would contend with the badge's own
  network for that one slot.
- **An app acting as a custom Shortcuts automation trigger source**:
  not possible at all — Shortcuts' Personal Automation trigger types
  (Time, Location, NFC, WiFi, Bluetooth connect/disconnect, Focus
  changes, etc.) are a fixed list Apple defines; there's no API for a
  third-party app to register a new trigger type, for the same
  abuse-prevention reason background apps can't call `open()` (design 3).
- **CoreBluetooth background `open()`, revisited with state restoration**:
  re-tested whether `UIApplication.open()` behaves differently when
  called from a CoreBluetooth background delegate callback (under the
  `bluetooth-central` background mode) rather than CLMonitor's
  location-based wake, on the theory that it's a different OS-granted
  execution context. Added `CBCentralManagerOptionRestoreIdentifierKey`
  state restoration to survive the app being killed while backgrounded.
  Result: state restoration never actually engaged (`bluetoothCentrals`
  never appeared as a launch option across many cycles) and a fresh
  connection attempt initiated from the background consistently never
  completed (no `didConnect`, no `didFailToConnect`) before the process
  was suspended — so the `open()` question was never even reached. Even
  setting that aside, the design-3 finding (backgrounded apps can never
  call `open()`, full stop) would have blocked it regardless.

The common thread across every one of these: the only things that
reliably survive a locked phone are **OS-level automation reacting to a
natively-recognized event type**, never a third-party app in the loop.
Full Keyboard Access's Commands feature is one such native trigger.
Design 7, below, found another.

## 7. Classic Bluetooth (BR/EDR) HID device + connect/disconnect

**The insight that reopened this:** design 6 concluded that Shortcuts'
"Bluetooth device connects/disconnects" automation likely only recognizes
**Classic Bluetooth (BR/EDR)**, not BLE — and every badge built so far,
including the XIAO ESP32C6, is BLE-only hardware with no Classic radio at
all, so this was never actually tested against a device that could prove
or disprove it. That changed when a real Classic-BT-capable accessory (a
DualShock controller) was manually added to the automation's device list
and confirmed to fire the automation reliably — direct proof the
automation isn't restricted to audio/car devices as design 6 might have
suggested, just to Classic Bluetooth generally.

**Design:** a plain ESP32-WROOM-32 (an AITRIP DevKit board with a CP2102
USB-UART bridge — not the XIAO ESP32C6, which has no BR/EDR radio) runs
as a genuine Classic Bluetooth HID device. Button-driven, same shape as
design 4/6: press while disconnected actively connects (or, before any
pairing has happened, goes connectable/discoverable so the phone can pair
via Settings → Bluetooth); press while connected actively disconnects;
after a disconnect it deliberately stays non-connectable/non-discoverable
until the next press — no auto-reconnect. A **native Shortcuts Bluetooth
automation** (identical in kind to the DualShock/headphone precedent)
reacts to the connect/disconnect events directly — no app, no Full
Keyboard Access.

**Toolchain note:** the Classic BT HID Device API
(`esp_bt_hid_device_*`, ESP-IDF's Bluedroid `bt` component) isn't exposed
by the Arduino-ESP32 core at all — it requires enabling "Classic BT HID
Device" in `idf.py menuconfig`, which Arduino's precompiled build doesn't
support changing. This firmware (`firmware/experiments/classic_bt_hid_switch/`)
is a full ESP-IDF project (`idf.py build/flash`), not an Arduino sketch —
a real departure from every other firmware in this repo, kept as an
ESP-IDF project specifically for this reason. Adapted from Espressif's
own official example, `examples/bluetooth/bluedroid/classic_bt/bt_hid_mouse_device`.

**Debugging along the way:**
- First attempt presented as a **Mouse**-class HID device (matching the
  official example's own device class, on the assumption any working
  Classic BT HID class would do) — it never appeared in Settings →
  Bluetooth's Other Devices list at all, even waiting a full minute with
  the screen open. Switched to **Keyboard** class (COD minor +
  descriptor + subclass), matching both our own already-proven BLE
  keyboard design and the DualShock/headphone precedent (game
  controller and audio, not a generic pointing device) — it appeared
  immediately. iOS's Classic BT pairing UI appears not to support
  generic HID mice the way it does keyboards and game controllers.
- Pairing a Keyboard-class device while Full Keyboard Access happened to
  still be enabled (left on from design 5 testing earlier the same day)
  brought back the same persistent highlight box — a red herring, not a
  new problem: FKA reacts to *any* connected external keyboard
  regardless of what triggered the pairing. Turning FKA off (it isn't
  needed by this design at all — no keystrokes are ever sent, only
  connect/disconnect events) confirmed the highlight has nothing to do
  with this design specifically.

**Result: confirmed working end-to-end, including with the phone
locked** — connect and disconnect each fire the Shortcuts automation
reliably, with Full Keyboard Access off and no app involved at any point.
This is the first design in the whole investigation that is
simultaneously hands-free, works locked, needs no app, and needs no
Accessibility feature with an ongoing UX cost.

**Hardware implication:** this design needs a Classic-BT-capable chip
(original ESP32, ESP32-S3, etc.), not the BLE-only XIAO ESP32C6 the
project started with. See CLAUDE.md for the updated hardware section.

## Summary table

| # | Design | Background/locked result |
|---|---|---|
| 1 | Custom BLE broadcast | Foreground only; no background path at all |
| 2 | iBeacon + CoreLocation region monitoring | Foreground works; background never delivered |
| 3 | iBeacon + CLMonitor | Background detection works; can't act on it (app can't open Shortcuts) |
| 4 | Bonded connect/disconnect + Shortcuts automation (Heart Rate masquerade) | Automation fires fine while locked; badge never gets reconnect-capable |
| 5 | HID keyboard, real keystrokes + Full Keyboard Access | Works, but unreliable on a single attempt while locked |
| 6 | HID classification + connect/disconnect (no keystrokes) | Connection works; automation never recognizes it at all |
| 5, hardened | HID keystrokes, repeated idempotently | Works reliably, but Full Keyboard Access's persistent highlight is real ongoing UX friction |
| (detours) | HomeKit, WiFi automation, app-as-trigger, CoreBluetooth background `open()` | All dead ends — see above |
| **7** | **Classic BT HID (Keyboard class) + connect/disconnect, native Bluetooth automation** | **Works reliably, hands-free, no app, no Full Keyboard Access** |
