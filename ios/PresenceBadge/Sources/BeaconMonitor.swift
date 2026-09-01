import CoreLocation
import Combine

/// Watches for the badge's iBeacon identity and reacts to state changes.
/// Protocol details: docs/ble-protocol.md in the repo root.
///
/// Uses CoreLocation region monitoring rather than a raw CoreBluetooth
/// scan, because iOS reliably wakes an app on region enter/exit even from
/// the background or a terminated state — raw CB background scanning is
/// throttled too heavily to be usable for this badge's actual purpose
/// (working while the phone is locked/pocketed). See ble-protocol.md's
/// "Why iBeacon" section for the full reasoning.
///
/// Important limitation: background region-entry events carry no RSSI —
/// CoreLocation just reports "this identity is now in range," with no
/// signal-strength gate available. The RSSI proximity filter only applies
/// while foreground ranging is active. Fine while there's a single badge
/// (see CLAUDE.md); revisit once more than one badge exists.
final class BeaconMonitor: NSObject, ObservableObject {
    private static let badgeUUID = UUID(uuidString: "651BBFEC-F197-444E-BF25-D72C1D4CCD84")!
    private static let major: CLBeaconMajorValue = 1
    private static let minorOn: CLBeaconMinorValue = 1
    private static let minorOff: CLBeaconMinorValue = 0

    private static let onConstraint = CLBeaconIdentityConstraint(uuid: badgeUUID, major: major, minor: minorOn)
    private static let offConstraint = CLBeaconIdentityConstraint(uuid: badgeUUID, major: major, minor: minorOff)
    private static let onRegion = CLBeaconRegion(beaconIdentityConstraint: onConstraint, identifier: "BadgeFocusOn")
    private static let offRegion = CLBeaconRegion(beaconIdentityConstraint: offConstraint, identifier: "BadgeFocusOff")

    /// Minimum RSSI to act on a *foreground* sighting — see the class-level
    /// note above on why this can't apply to background events.
    @Published var rssiThreshold: Int = -60

    @Published private(set) var authorizationStatus: CLAuthorizationStatus = .notDetermined
    @Published private(set) var isRanging = false
    @Published private(set) var lastRSSI: Int?
    @Published private(set) var badgeFocusState: Bool?
    @Published private(set) var lastEventDescription = "No badge seen yet"

    private let locationManager = CLLocationManager()
    private var lastAppliedState: Bool?
    private let focusTrigger = FocusTrigger()

    override init() {
        super.init()
        locationManager.delegate = self
        authorizationStatus = locationManager.authorizationStatus
    }

    func requestAuthorizationAndStartMonitoring() {
        locationManager.requestAlwaysAuthorization()
    }

    private func startMonitoring() {
        locationManager.startMonitoring(for: Self.onRegion)
        locationManager.startMonitoring(for: Self.offRegion)
        locationManager.requestState(for: Self.onRegion)
        locationManager.requestState(for: Self.offRegion)
    }

    /// Call when the app becomes active — ranging gives live RSSI for the
    /// proximity filter and the status UI, but only makes sense to run
    /// while foregrounded.
    func startForegroundRanging() {
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
            : "Applied focus \(state ? "ON" : "OFF") (background region event, no RSSI)"
        focusTrigger.setFocus(on: state)
    }
}

extension BeaconMonitor: CLLocationManagerDelegate {
    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        authorizationStatus = manager.authorizationStatus
        if authorizationStatus == .authorizedAlways || authorizationStatus == .authorizedWhenInUse {
            startMonitoring()
        }
    }

    func locationManager(_ manager: CLLocationManager, didEnterRegion region: CLRegion) {
        guard let beaconRegion = region as? CLBeaconRegion else { return }
        apply(state: beaconRegion.identifier == "BadgeFocusOn", rssi: nil)
    }

    func locationManager(_ manager: CLLocationManager, didDetermineState state: CLRegionState, for region: CLRegion) {
        guard state == .inside, let beaconRegion = region as? CLBeaconRegion else { return }
        apply(state: beaconRegion.identifier == "BadgeFocusOn", rssi: nil)
    }

    func locationManager(
        _ manager: CLLocationManager,
        didRange beacons: [CLBeacon],
        satisfying constraint: CLBeaconIdentityConstraint
    ) {
        guard let beacon = beacons.first, beacon.rssi != 0 else { return }
        let state = constraint.minor == Self.minorOn
        apply(state: state, rssi: beacon.rssi)
    }
}
