# BLE HID protocol (phase 1)

The badge is a BLE HID keyboard (device name `PresenceBadge`), built with
the `HijelHID_BLEKeyboard` library. It stays continuously connected once
paired — like a real Bluetooth keyboard — rather than toggling
connect/disconnect per press. There's no advertised state payload; the
signal is the keystroke itself.

## How it works

- **Pairing:** once, via Settings → Bluetooth. Standard HID "Just Works"
  pairing (no PIN).
- **Button press "on":** sends keystroke **F13**.
- **Button press "off":** sends keystroke **F14**.
- F13/F14 are otherwise-unused function keys, chosen specifically so they
  can't collide with normal typing.

## Phone-side trigger

Not a Shortcuts automation — a native OS-level key binding, with no app
in the loop at all:

**Settings → Accessibility → Keyboards & Typing → Full Keyboard Access →
Commands** → bind F13 to the **Badge Focus On** shortcut, F14 to
**Badge Focus Off**. Confirmed empirically (with a real Bluetooth
accessory, before committing to this design) that this class of
OS-level trigger fires reliably even while the phone is locked. See
`ios/README.md`.

## Why HID specifically (superseded designs)

Two earlier designs were tried and abandoned — full history in CLAUDE.md's
Architecture decisions:

1. **Broadcast-only, then iBeacon + CoreLocation** (region monitoring,
   then the newer `CLMonitor` API) — chosen to get reliable background
   detection via a custom app. Neither actually delivered background
   events reliably in practice, and `CLMonitor` additionally hit a hard
   OS wall: a backgrounded app can't call `UIApplication.open(_:)` to
   hand off to Shortcuts at all.
2. **Bonded BLE connect/disconnect, reacted to by a native Shortcuts
   Bluetooth automation** — worked for the "automation fires while
   locked" part, but the badge itself needed Settings → Bluetooth
   visibility (worked around by masquerading as a Heart Rate device,
   since Apple's own `AccessorySetupKit` needs an entitlement Apple has
   to approve, not available for this prototype) and, more fundamentally,
   iOS doesn't auto-reconnect a regular custom BLE peripheral without an
   app running — only recognized device classes (HID, audio) get that
   treatment app-free. That's exactly why this design uses real HID
   instead of working around the connect/disconnect mechanism.

## Not decided yet / revisit later

- Per-badge unique identity, once more than one badge exists (see
  CLAUDE.md) — would need a distinct device name/keystroke pair per
  badge, and a way for each wearer's Full Keyboard Access bindings to
  point at their own badge specifically.
