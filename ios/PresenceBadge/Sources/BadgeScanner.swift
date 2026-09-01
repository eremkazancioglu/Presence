import CoreBluetooth
import Combine

/// Scans for the Bedside Focus Badge's BLE advertisement and reacts to
/// state changes. Protocol details: docs/ble-protocol.md in the repo root.
///
/// Foreground-only for now — background BLE scanning has real iOS
/// restrictions that are a separate problem to solve once this works
/// reliably in the foreground.
final class BadgeScanner: NSObject, ObservableObject {
    private static let deviceName = "PresenceBadge"
    private static let manufacturerID: UInt16 = 0xFFFF
    private static let protocolMagic: UInt8 = 0x50

    /// Minimum RSSI to treat a sighting as "this is my badge, worn on my
    /// own body" rather than someone else's badge nearby. Signal strength
    /// through a body is noisy — start here and recalibrate against the
    /// real badge once testing starts.
    @Published var rssiThreshold: Int = -60

    @Published private(set) var isScanning = false
    @Published private(set) var lastRSSI: Int?
    @Published private(set) var badgeFocusState: Bool?
    @Published private(set) var lastEventDescription = "No badge seen yet"

    private var centralManager: CBCentralManager!
    private var lastAppliedState: Bool?
    private let focusTrigger = FocusTrigger()

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    private func startScanningIfPoweredOn() {
        guard centralManager.state == .poweredOn else { return }
        centralManager.scanForPeripherals(withServices: nil, options: [
            CBCentralManagerScanOptionAllowDuplicatesKey: true
        ])
        isScanning = true
    }

    private func parseState(from advertisementData: [String: Any]) -> Bool? {
        guard let mfgData = advertisementData[CBAdvertisementDataManufacturerDataKey] as? Data,
              mfgData.count >= 4 else { return nil }

        let companyID = UInt16(mfgData[0]) | (UInt16(mfgData[1]) << 8)
        guard companyID == Self.manufacturerID,
              mfgData[2] == Self.protocolMagic else { return nil }

        return mfgData[3] == 0x01
    }
}

extension BadgeScanner: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        if central.state == .poweredOn {
            startScanningIfPoweredOn()
        } else {
            isScanning = false
        }
    }

    func centralManager(
        _ central: CBCentralManager,
        didDiscover peripheral: CBPeripheral,
        advertisementData: [String: Any],
        rssi RSSI: NSNumber
    ) {
        guard let name = advertisementData[CBAdvertisementDataLocalNameKey] as? String,
              name == Self.deviceName else { return }
        guard let state = parseState(from: advertisementData) else { return }

        lastRSSI = RSSI.intValue
        badgeFocusState = state

        guard RSSI.intValue >= rssiThreshold else {
            lastEventDescription = "Saw badge (RSSI \(RSSI.intValue), below threshold — ignored)"
            return
        }

        guard state != lastAppliedState else { return }
        lastAppliedState = state

        lastEventDescription = "Applied focus \(state ? "ON" : "OFF") (RSSI \(RSSI.intValue))"
        focusTrigger.setFocus(on: state)
    }
}
