# Presence Badge companion app (iOS)

Minimal SwiftUI app that watches for the badge's iBeacon identity and
toggles Focus mode. See [../docs/ble-protocol.md](../docs/ble-protocol.md)
for the advertising format and why it's iBeacon rather than a custom BLE
scheme (short version: CoreLocation region monitoring is the only
mechanism iOS provides that reliably wakes an app in the background/from
terminated state — plain CoreBluetooth background scanning is too
throttled to be usable for a badge meant to work while the phone is
locked/pocketed).

**Status:** builds and runs. Verified against the real badge in the
foreground; background/locked-phone behavior not yet validated on device.

## Why it needs Location permission

CoreLocation's beacon region monitoring is the underlying mechanism (see
above) — this app never reads or stores actual location data, but iOS
requires "Always" location authorization for any app that monitors beacon
regions in the background. `BeaconMonitor.swift` requests it on first
launch after onboarding.

## Why it needs two Shortcuts installed

Third-party apps have no public API to toggle system Focus mode directly.
The only way is Shortcuts' own **Set Focus** action, so this app detects
the badge's state and runs a pre-built shortcut (`Badge Focus On` /
`Badge Focus Off`) via the `shortcuts://x-callback-url/run-shortcut` URL
scheme (the x-callback-url variant hands control back to this app once the
shortcut finishes, instead of leaving the user in the Shortcuts app). The
app's onboarding screen walks you through installing them — but the
`.shortcut` files themselves must be hand-built once in the Shortcuts app
first; see
[PresenceBadge/Resources/Shortcuts/README.md](PresenceBadge/Resources/Shortcuts/README.md).

## Building

```sh
brew install xcodegen   # one-time
cd ios/PresenceBadge
xcodegen generate       # regenerates PresenceBadge.xcodeproj from project.yml
open PresenceBadge.xcodeproj
```

Then in Xcode: select your iPhone as the run destination (Bluetooth/beacon
detection doesn't work in the Simulator), set your Apple ID under
Signing & Capabilities if prompted, and Run. With free (non-paid) Apple ID
signing, the app needs reinstalling from Xcode roughly every 7 days.

For running without a cable each time: Xcode -> Window -> Devices and
Simulators -> select your iPhone (after at least one USB connection) ->
enable network connection (exact wording/location varies by Xcode
version — check the device's context menu if it's not visible directly).

`PresenceBadge.xcodeproj` and `Resources/Info.plist` are generated —
they're gitignored; edit `project.yml` and rerun `xcodegen generate`
instead of editing them directly.

Verifying compiles from the command line (no Xcode GUI needed):

```sh
xcodebuild -project PresenceBadge.xcodeproj -scheme PresenceBadge \
  -destination 'generic/platform=iOS Simulator' -sdk iphonesimulator \
  build CODE_SIGNING_ALLOWED=NO
```

## Calibrating the RSSI threshold

The in-app "Proximity filter" stepper (default -60 dBm) only applies while
the app is in the foreground and actively ranging — background region
events (phone locked/backgrounded) carry no signal-strength data, so
there's nothing to filter there. Watch the Status section in the
foreground while wearing the badge at a normal distance vs. across a room,
and adjust until it reliably distinguishes the two. This is a coarse
proximity heuristic, not per-badge identity — fine while there's a single
badge; see CLAUDE.md's open questions for what changes once more exist.
