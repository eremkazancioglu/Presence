import CoreLocation
import Combine

/// Watches for the badge's iBeacon identity and reacts to state changes.
/// Protocol details: docs/ble-protocol.md in the repo root.
///
/// Background wake uses `CLMonitor` (iOS 17+), not the older
/// `CLLocationManager.startMonitoring(for: CLBeaconRegion)` +
/// delegate-callback mechanism. Both were tried: the legacy region
/// monitoring registered correctly and worked reliably in the foreground
/// (didEnterRegion/didDetermineState fired as expected), but delivered
/// nothing at all in the background after extended testing (5+ minutes,
/// permissions/accuracy/Background App Refresh/Low Power Mode all ruled
/// out as causes). CLMonitor is Apple's newer replacement subsystem for
/// condition-based background monitoring and is what's used here instead.
///
/// Foreground live status/RSSI still comes from `startRangingBeacons`
/// (CLLocationManager), which is unrelated to and unaffected by this
/// change -- that was already confirmed working.
///
/// Important limitation: background events carry no RSSI. The RSSI
/// proximity filter only applies while foreground ranging is active. Fine
/// while there's a single badge (see CLAUDE.md); revisit once more than
/// one badge exists.
final class BeaconMonitor: NSObject, ObservableObject {
    private static let badgeUUID = UUID(uuidString: "651BBFEC-F197-444E-BF25-D72C1D4CCD84")!
    private static let major: CLBeaconMajorValue = 1
    private static let minorOn: CLBeaconMinorValue = 1
    private static let minorOff: CLBeaconMinorValue = 0

    private static let onConstraint = CLBeaconIdentityConstraint(uuid: badgeUUID, major: major, minor: minorOn)
    private static let offConstraint = CLBeaconIdentityConstraint(uuid: badgeUUID, major: major, minor: minorOff)

    private static let onIdentifier = "BadgeFocusOn"
    private static let offIdentifier = "BadgeFocusOff"
    private static let monitorName = "com.eremkazancioglu.PresenceBadge.monitor"

    /// Minimum RSSI to act on a *foreground* sighting — see the class-level
    /// note above on why this can't apply to background events.
    @Published var rssiThreshold: Int = -60

    @Published private(set) var authorizationStatus: CLAuthorizationStatus = .notDetermined
    @Published private(set) var accuracyAuthorization: CLAccuracyAuthorization = .reducedAccuracy
    @Published private(set) var isRanging = false
    @Published private(set) var lastRSSI: Int?
    @Published private(set) var badgeFocusState: Bool?
    @Published private(set) var lastEventDescription = "No badge seen yet"

    private let locationManager = CLLocationManager()
    private var lastAppliedState: Bool?
    private let focusTrigger = FocusTrigger()
    private var monitorTask: Task<Void, Never>?

    override init() {
        super.init()
        locationManager.delegate = self
        authorizationStatus = locationManager.authorizationStatus
        accuracyAuthorization = locationManager.accuracyAuthorization
        print("[BeaconMonitor] init: auth=\(authorizationStatus.rawValue) accuracy=\(accuracyAuthorization == .fullAccuracy ? "full" : "reduced") rangingAvailable=\(CLLocationManager.isRangingAvailable())")
    }

    func requestAuthorizationAndStartMonitoring() {
        locationManager.requestAlwaysAuthorization()
    }

    /// Beacon ranging/monitoring silently produces nothing at all under
    /// "Approximate" location (a separate per-app toggle from
    /// Always/When-In-Use, Settings -> Privacy -> Location Services ->
    /// [app] -> Precise Location). This requests a temporary upgrade to
    /// full accuracy, which is the Apple-sanctioned way to ask -- there's
    /// no way to force it permanently from code, only guide the user to
    /// the Settings toggle if they decline repeatedly.
    private func requestFullAccuracyIfNeeded() {
        guard accuracyAuthorization == .reducedAccuracy else { return }
        locationManager.requestTemporaryFullAccuracyAuthorization(withPurposeKey: "BeaconRanging") { [weak self] _ in
            guard let self else { return }
            self.accuracyAuthorization = self.locationManager.accuracyAuthorization
        }
    }

    private func startBackgroundMonitoring() {
        monitorTask?.cancel()
        monitorTask = Task { [weak self] in
            guard let self else { return }
            let monitor = await CLMonitor(Self.monitorName)

            let onCondition = CLMonitor.BeaconIdentityCondition(uuid: Self.badgeUUID, major: Self.major, minor: Self.minorOn)
            let offCondition = CLMonitor.BeaconIdentityCondition(uuid: Self.badgeUUID, major: Self.major, minor: Self.minorOff)
            await monitor.add(onCondition, identifier: Self.onIdentifier)
            await monitor.add(offCondition, identifier: Self.offIdentifier)
            print("[BeaconMonitor] CLMonitor conditions registered")

            do {
                for try await event in await monitor.events {
                    print("[BeaconMonitor] CLMonitor event: \(event.identifier) state=\(event.state)")
                    guard event.state == .satisfied else { continue }
                    await MainActor.run {
                        self.apply(state: event.identifier == Self.onIdentifier, rssi: nil)
                    }
                }
            } catch {
                print("[BeaconMonitor] CLMonitor events stream error: \(error)")
            }
        }
    }

    /// Call when the app becomes active — ranging gives live RSSI for the
    /// proximity filter and the status UI, but only makes sense to run
    /// while foregrounded.
    func startForegroundRanging() {
        print("[BeaconMonitor] startForegroundRanging called")
        locationManager.startRangingBeacons(satisfying: Self.onConstraint)
        locationManager.startRangingBeacons(satisfying: Self.offConstraint)
        isRanging = true
    }

    func stopForegroundRanging() {
        locationManager.stopRangingBeacons(satisfying: Self.onConstraint)
        locationManager.stopRangingBeacons(satisfying: Self.offConstraint)
        isRanging = false
    }

    /// - Parameter rssi: nil for background monitoring events (no signal
    ///   strength available); an actual RSSI for foreground ranging, which
    ///   gates on `rssiThreshold`.
    private func apply(state: Bool, rssi: Int?) {
        badgeFocusState = state
        if let rssi { lastRSSI = rssi }

        if let rssi, rssi < rssiThreshold {
            lastEventDescription = "Saw badge (RSSI \(rssi), below threshold — ignored)"
            return
        }

        guard state != lastAppliedState else { return }
        lastAppliedState = state

        lastEventDescription = rssi != nil
            ? "Applied focus \(state ? "ON" : "OFF") (RSSI \(rssi!))"
            : "Applied focus \(state ? "ON" : "OFF") (background CLMonitor event, no RSSI)"
        focusTrigger.setFocus(on: state)
    }
}

extension BeaconMonitor: CLLocationManagerDelegate {
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        authorizationStatus = manager.authorizationStatus
        accuracyAuthorization = manager.accuracyAuthorization
        print("[BeaconMonitor] didChangeAuthorization: auth=\(authorizationStatus.rawValue) accuracy=\(accuracyAuthorization == .fullAccuracy ? "full" : "reduced")")
        if authorizationStatus == .authorizedAlways || authorizationStatus == .authorizedWhenInUse {
            requestFullAccuracyIfNeeded()
            startBackgroundMonitoring()
            // Also (re)start ranging here, not just on scenePhase changes:
            // this delegate callback fires once per launch regardless of
            // whether the app's scenePhase actually *transitions* (it
            // doesn't, if the app launches straight into .active), so this
            // is the one reliably-fired hook to kick off ranging on a
            // fresh foreground launch.
            startForegroundRanging()
        }
    }

    func locationManager(
        _ manager: CLLocationManager,
        didRange beacons: [CLBeacon],
        satisfying constraint: CLBeaconIdentityConstraint
    ) {
        print("[BeaconMonitor] didRange: \(beacons.count) beacon(s) for minor=\(constraint.minor?.description ?? "any")")
        guard let beacon = beacons.first, beacon.rssi != 0 else { return }
        let state = constraint.minor == Self.minorOn
        apply(state: state, rssi: beacon.rssi)
    }

    func locationManager(
        _ manager: CLLocationManager,
        didFailRangingFor constraint: CLBeaconIdentityConstraint,
        error: Error
    ) {
        print("[BeaconMonitor] didFailRangingFor minor=\(constraint.minor?.description ?? "any"): \(error)")
    }

    func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        print("[BeaconMonitor] didFailWithError: \(error)")
    }
}
