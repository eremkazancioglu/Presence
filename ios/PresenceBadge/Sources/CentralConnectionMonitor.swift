import CoreBluetooth
import UIKit

/// EXPERIMENT: tests whether `UIApplication.open()` succeeds when called
/// straight from a CoreBluetooth background delegate callback (under the
/// `bluetooth-central` background mode) -- a different OS-granted
/// background execution context than `BeaconMonitor`'s CLMonitor-based
/// wake, which hit a hard wall (`LSApplicationWorkspaceErrorDomain` code
/// 115) despite the beacon event itself delivering reliably in the
/// background. See docs/trigger-mechanism-investigation.md.
///
/// Not wired into the production trigger path -- this is a standalone
/// test, paired with firmware/experiments/central_bg_test/. Logs to both
/// `print` (Xcode console, if still attached) and a file, since the
/// console may not stay attached once the phone is locked/unplugged.
final class CentralConnectionMonitor: NSObject, ObservableObject {
    // Must match central_bg_test.ino's TEST_SERVICE_UUID.
    private static let testServiceUUID = CBUUID(string: "8E400001-F315-4F60-9FB8-838830DAEA50")

    // Stable across launches so bluetoothd can match a restored session
    // back to this app -- required for state restoration to work at all.
    private static let restoreIdentifier = "PresenceBadgeCentralTest"

    @Published private(set) var statusDescription = "Not started"

    private var centralManager: CBCentralManager?

    private let focusTrigger = FocusTrigger()

    private static let logFileURL: URL = {
        let dir = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
        return dir.appendingPathComponent("central-test.log")
    }()

    /// Static + callable from AppDelegate, which runs before any
    /// CentralConnectionMonitor instance necessarily exists (e.g. an
    /// OS-triggered background relaunch for Bluetooth state restoration).
    static func staticLog(_ message: String) {
        let state: String
        switch UIApplication.shared.applicationState {
        case .active: state = "active"
        case .inactive: state = "inactive"
        case .background: state = "background"
        @unknown default: state = "unknown"
        }
        let line = "[\(Date())] (appState=\(state)) \(message)"
        print("[CentralTest] \(line)")
        guard let data = (line + "\n").data(using: .utf8) else { return }
        if FileManager.default.fileExists(atPath: logFileURL.path) {
            if let handle = try? FileHandle(forWritingTo: logFileURL) {
                handle.seekToEndOfFile()
                handle.write(data)
                try? handle.close()
            }
        } else {
            try? data.write(to: logFileURL)
        }
    }

    /// Must be called unconditionally, as early in launch as possible --
    /// including on a background relaunch the OS triggers to restore this
    /// central manager. A restoration identifier only reconnects to the
    /// OS-level session if a CBCentralManager with the same identifier is
    /// (re)created during that launch; waiting for a user tap misses it.
    func start() {
        guard centralManager == nil else { return }
        log("start() called")
        centralManager = CBCentralManager(
            delegate: self,
            queue: nil,
            options: [CBCentralManagerOptionRestoreIdentifierKey: Self.restoreIdentifier]
        )
    }

    private func log(_ message: String) {
        Self.staticLog(message)
        statusDescription = message
    }

    func readLog() -> String {
        (try? String(contentsOf: Self.logFileURL, encoding: .utf8)) ?? "(empty)"
    }

    func clearLog() {
        try? FileManager.default.removeItem(at: Self.logFileURL)
        statusDescription = "Log cleared"
    }
}

extension CentralConnectionMonitor: CBCentralManagerDelegate {
    /// Fires when the OS relaunches/reconnects this central manager after
    /// the app process was killed -- proof state restoration is actually
    /// engaging, independent of whether didConnect/didDisconnect ever get
    /// this far.
    func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        let peripheralCount = (dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral])?.count ?? 0
        log("willRestoreState -- restored \(peripheralCount) peripheral(s)")
    }

    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        log("centralManagerDidUpdateState: \(central.state.rawValue)")
        guard central.state == .poweredOn else { return }
        central.scanForPeripherals(withServices: [Self.testServiceUUID])
        log("Scanning for test badge...")
    }

    func centralManager(_ central: CBCentralManager, didDiscover peripheral: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        log("Discovered \(peripheral.identifier), connecting...")
        central.stopScan()
        central.connect(peripheral)
    }

    func centralManager(_ central: CBCentralManager, didConnect peripheral: CBPeripheral) {
        log("didConnect -- calling FocusTrigger.setFocus(on: true)")
        focusTrigger.setFocus(on: true)
    }

    func centralManager(_ central: CBCentralManager, didDisconnectPeripheral peripheral: CBPeripheral, error: Error?) {
        log("didDisconnectPeripheral -- calling FocusTrigger.setFocus(on: false), resuming scan")
        focusTrigger.setFocus(on: false)
        central.scanForPeripherals(withServices: [Self.testServiceUUID])
    }

    func centralManager(_ central: CBCentralManager, didFailToConnect peripheral: CBPeripheral, error: Error?) {
        log("didFailToConnect: \(error?.localizedDescription ?? "unknown")")
    }
}
