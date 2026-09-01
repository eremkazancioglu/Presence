# Presence Badge companion app (iOS)

Minimal SwiftUI app that scans for the badge's BLE advertisement and
toggles Focus mode. See [../docs/ble-protocol.md](../docs/ble-protocol.md)
for the advertising format this reads.

**Status:** builds and runs, RSSI-based proximity filtering only (no
per-badge identity yet — fine while there's a single badge; revisit once
more than one exists, see CLAUDE.md). Foreground-only — background BLE
scanning is a separate problem not yet tackled.

## Why it needs two Shortcuts installed

Third-party apps have no public API to toggle system Focus mode directly.
The only way is Shortcuts' own **Set Focus** action, so this app detects
the badge's state and runs a pre-built shortcut (`Badge Focus On` /
`Badge Focus Off`) via the `shortcuts://run-shortcut` URL scheme. The app's
onboarding screen walks you through installing them — but the `.shortcut`
files themselves must be hand-built once in the Shortcuts app first; see
[PresenceBadge/Resources/Shortcuts/README.md](PresenceBadge/Resources/Shortcuts/README.md).

## Building

```sh
brew install xcodegen   # one-time
cd ios/PresenceBadge
xcodegen generate       # regenerates PresenceBadge.xcodeproj from project.yml
open PresenceBadge.xcodeproj
```

Then in Xcode: select your iPhone as the run destination (Bluetooth
doesn't work in the Simulator), set your Apple ID under
Signing & Capabilities if prompted, and Run. With free (non-paid) Apple ID
signing, the app needs reinstalling from Xcode roughly every 7 days.

`PresenceBadge.xcodeproj` and `Resources/Info.plist` are generated —
they're gitignored; edit `project.yml` and rerun `xcodegen generate`
instead of editing them directly.

## Calibrating the RSSI threshold

The in-app "Proximity filter" stepper (default -60 dBm) controls how close
the badge must read to trigger Focus. Signal strength through a worn badge
is noisy — watch the Status section while wearing the badge at a normal
distance vs. across a room, and adjust until it reliably distinguishes the
two.
