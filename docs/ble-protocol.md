# BLE connection protocol (phase 1)

The badge is a bonded, connectable BLE peripheral (device name
`PresenceBadge`) — not a broadcast-only beacon. The button drives
connect/disconnect against the phone it's bonded to; there's no
advertised state payload to parse.

## How it works

- **Bonding:** happens once, when the phone first pairs with the badge in
  Settings → Bluetooth. Uses "Just Works" pairing (bonding on, no MITM
  passkey — the badge has no display/keypad to show one) with LE Secure
  Connections.
- **Button press "on":** the badge starts (and keeps) connectable
  advertising. Since the phone already trusts this bonded device, iOS
  auto-reconnects as soon as it sees the advertisement — that reconnection
  is the "device connects" event.
- **Button press "off":** the badge actively disconnects (a live
  connection isn't advertising anyway, so merely stopping advertising
  wouldn't drop it) and stops advertising, so nothing reconnects until the
  next "on" press.
- **Self-healing mid-visit drops:** the badge stays advertising-connectable
  for the *entire* ON duration, not just at the moment of the press. If
  the connection drops unexpectedly (RF interference, brief range loss),
  iOS's bonded auto-reconnect completes again on its own — no extra button
  press needed. This shows up as a brief disconnect-then-reconnect
  flicker, not a silent stuck-off failure.

## Phone-side trigger

A native Shortcuts personal automation, not a custom app:
**Automation → When Bluetooth device "PresenceBadge" connects → Run
immediately → [Badge Focus On shortcut]**, and the mirror image for
disconnects → Focus Off. See `ios/README.md`.

## Why not a broadcast/iBeacon payload (superseded design)

Earlier phase 1 firmware broadcast a custom manufacturer-data payload,
then switched to standard iBeacon format specifically to use iOS's
CoreLocation region monitoring / `CLMonitor` for reliable background
detection. Neither actually delivered background events reliably in
practice on real hardware, despite correct setup (see CLAUDE.md's
Architecture decisions for the full investigation). A system-level
Bluetooth connection (this design) is managed by iOS itself, the same way
AirPods or an Apple Watch stay connected, and isn't subject to the
custom-app background-suspension problems that broke the earlier
approach.

## Not decided yet / revisit later

- Whether a GATT service becomes useful for something beyond bare
  connect/disconnect (e.g. battery level). Not needed for phase 1 — the
  badge advertises no custom service, just the default Generic
  Access/Attribute services NimBLE provides.
- Per-badge unique identity, once more than one badge exists (see
  CLAUDE.md).
