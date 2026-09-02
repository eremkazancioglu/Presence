# BLE HID protocol (phase 1)

The badge is a BLE HID keyboard (device name `PresenceBadge`), built with
the `HijelHID_BLEKeyboard` library. It stays continuously connected once
paired — like a real Bluetooth keyboard — rather than toggling
connect/disconnect per press. There's no advertised state payload; the
signal is the keystroke itself.

## How it works

- **Pairing:** once, via Settings → Bluetooth. Standard HID "Just Works"
  pairing (no PIN).
- **Button press "on":** sends **Ctrl+Option+O**.
- **Button press "off":** sends **Ctrl+Option+F**.
- Standard letter keys with modifiers — not F13/F14 as originally tried;
  those weren't reliably distinguished by iOS's Full Keyboard Access
  command recorder (both binding attempts recorded identically, blocking
  the second assignment).
- **Each press sends its command up to 4 times**, not once. Delivery to a
  locked/sleeping phone isn't reliable on a single attempt (external
  keyboard input received while locked doesn't get processed outright —
  it redirects to the Face ID/passcode screen — though empirically this
  is intermittent rather than an absolute block). Since Set Focus is
  idempotent, resending the same target state is safe and just costs a
  few extra seconds on the first press after the phone's been idle. Each
  attempt is preceded by an unbound "wake" keystroke (harmless even if it
  does get processed) and a real settle delay.

## Phone-side trigger

Not a Shortcuts automation — a native OS-level key binding, with no app
in the loop at all:

**Settings → Accessibility → Keyboards & Typing → Full Keyboard Access →
Commands** → bind Ctrl+Option+O to the **Badge Focus On** shortcut,
Ctrl+Option+F to **Badge Focus Off**. Confirmed empirically (with a real
Bluetooth accessory, before committing to this design) that this class of
OS-level trigger fires reliably even while the phone is locked. Both
shortcuts need **Allow Running While Locked** turned on (shortcut's own
(i) info screen). See `ios/README.md`.

## Why this design (full investigation)

Five other designs were tried and abandoned before this one — full
narrative with evidence in
[trigger-mechanism-investigation.md](trigger-mechanism-investigation.md),
condensed version in CLAUDE.md's Architecture decisions. Short version:

1. **Broadcast-only, then iBeacon + CoreLocation** (region monitoring,
   then the newer `CLMonitor` API) via a custom iOS app — neither
   delivered background events reliably, and `CLMonitor` additionally hit
   a hard OS wall: a backgrounded app can't open another app (Shortcuts)
   via URL at all.
2. **Bonded BLE connect/disconnect + native Shortcuts Bluetooth
   automation** (badge masquerading as a Heart Rate device for Settings
   pairing visibility) — the automation itself does fire reliably while
   locked (verified with a real headphone accessory), but a generic BLE
   peripheral doesn't get iOS's app-free auto-reconnect treatment, only
   recognized classes like HID do.
3. **Real HID classification + connect/disconnect, no keystrokes** — got
   proper auto-reconnect, but the Shortcuts automation never recognized
   the connection events at all, even with "Any Device" selected —
   probably because Shortcuts' Bluetooth automation only recognizes
   Classic Bluetooth, and the XIAO ESP32C6 is BLE-only hardware.
4. **Real HID keystrokes, sent once** — this is the current design's
   ancestor; worked, but unreliably on a single delivery attempt while
   locked, fixed by the redundant-send approach described above.

## Not decided yet / revisit later

- Per-badge unique identity, once more than one badge exists (see
  CLAUDE.md) — would need a distinct device name/key-binding pair per
  badge, and a way for each wearer's Full Keyboard Access bindings to
  point at their own badge specifically.
