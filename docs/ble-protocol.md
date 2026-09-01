# BLE advertising protocol (phase 1)

The badge advertises as a standard **iBeacon** — not a custom manufacturer
data scheme (see "Why iBeacon, not custom manufacturer data" below).

## Advertisement contents

- **UUID:** `651BBFEC-F197-444E-BF25-D72C1D4CCD84` — fixed, identifies this
  badge design. (If more badges are built later, this is the natural
  per-badge identity field — see CLAUDE.md's open questions.)
- **Major:** `1` — fixed for now, unused, reserved for future
  differentiation (e.g. hardware revision).
- **Minor:** the on/off state.
  - `0` = focus OFF
  - `1` = focus ON
- **Tx power byte:** `0xC5` (-59 dBm), the standard iBeacon calibration
  reference value — not calibrated against this specific badge, fine for
  our purposes since we don't rely on iBeacon's distance estimate.

The device also advertises the name `PresenceBadge` in the scan response
packet (not the main advertisement — the iBeacon payload alone fills
nearly the whole 31-byte legacy advertising packet), for human-readable
identification when scanning with any BLE tool.

Toggling: press the button, badge switches its advertised Minor value
between 0 and 1. The iOS app doesn't read this as a single "state changed"
event — it treats "beacon `...4C84`/Major 1/Minor 1 is present" as ON and
"Minor 0 is present" as OFF, and reacts to the region transition.

## Why iBeacon, not custom manufacturer data

The original design used a custom manufacturer data payload (company ID
`0xFFFF` + 2 bespoke bytes). That worked for foreground scanning but has no
reliable way to wake or notify the iOS app while it's backgrounded or the
phone is locked — iOS throttles raw CoreBluetooth background scanning
heavily (roughly one discovery callback per peripheral per scan cycle,
scan cycles slowed "dramatically"), with no guarantee a given advertisement
is ever seen.

iBeacon format is recognized by CoreLocation's **region monitoring**
(`CLBeaconRegion`), which iOS built specifically to reliably wake an app in
the background — even from a fully terminated state — when a beacon
identity appears/disappears. Not literally instant (real-world reports:
low tens of seconds) and not 100% guaranteed (rare relaunch-failure edge
cases are documented), but meaningfully more reliable than raw background
BLE scanning. Given the whole point of this badge is working while the
phone is locked/pocketed, that reliability matters more than instant
foreground latency.

Using region monitoring for a button-triggered signal (rather than actual
physical proximity) works because the badge is worn at an effectively
fixed, close distance from the phone at all times — what changes is *which
Minor value* the badge broadcasts (driven by the button), not the physical
distance. Region monitoring here is a delivery mechanism for a
button-driven state signal, not a proximity trigger.

## Not decided yet / revisit later

- Whether a real GATT service becomes necessary (e.g. battery level, ack).
  Not in scope for phase 1.
- Per-badge unique UUID/Major, once more than one badge exists (see
  CLAUDE.md).
