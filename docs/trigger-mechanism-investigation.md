# Trigger mechanism investigation

How the badge actually tells the phone to toggle Focus mode went through
six real designs before landing on one that works reliably. This
document walks through the full investigation in order: what we tried,
why we expected it to work, what we found, and why it failed (or didn't).
CLAUDE.md's "Architecture decisions" section has the condensed version;
this is the detailed history, useful if this project ever needs to be
revisited, ported to Android, or re-explained from scratch.

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

## Summary table

| # | Design | Background/locked result |
|---|---|---|
| 1 | Custom BLE broadcast | Foreground only; no background path at all |
| 2 | iBeacon + CoreLocation region monitoring | Foreground works; background never delivered |
| 3 | iBeacon + CLMonitor | Background detection works; can't act on it (app can't open Shortcuts) |
| 4 | Bonded connect/disconnect + Shortcuts automation (Heart Rate masquerade) | Automation fires fine while locked; badge never gets reconnect-capable |
| 5 | HID keyboard, real keystrokes + Full Keyboard Access | Works, but unreliable on a single attempt while locked |
| 6 | HID classification + connect/disconnect (no keystrokes) | Connection works; automation never recognizes it at all |
| **5, hardened** | HID keystrokes, repeated idempotently | **Works reliably** |
