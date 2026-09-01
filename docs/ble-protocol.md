# BLE advertising protocol (phase 1)

Advertising-only, no GATT service, no pairing. The badge broadcasts its
current on/off state continuously; any listening device (phone automation,
laptop scanner) reads it passively.

## Advertisement contents

- **Device name:** `PresenceBadge` (constant — does not change with state,
  so the packet doesn't need a full re-encode of the name on every toggle).
- **Manufacturer data:** company ID `0xFFFF` (Bluetooth SIG reserved value
  for prototyping/testing — not for production use), followed by 2 bytes:

  | Byte offset | Meaning        | Values                     |
  |-------------|----------------|-----------------------------|
  | 0           | Protocol magic | `0x50` (`'P'`)              |
  | 1           | Focus state    | `0x00` = OFF, `0x01` = ON   |

  Example payload (focus ON): `50 01`

## Why manufacturer data instead of encoding state in the name

Keeps the device name stable (easier to spot while scanning with any BLE
app) and keeps the state in a fixed, easy-to-parse byte for automations
(iOS Shortcuts BLE triggers, `bleak` scripts, etc.) rather than parsing a
changing string.

## Not decided yet / revisit later

- Whether company ID `0xFFFF` needs to change if this ever ships beyond a
  personal prototype (SIG reserves it for testing only).
- GATT service — only needed if two-way communication (e.g. battery level,
  ack) becomes necessary. Not in scope for phase 1.
