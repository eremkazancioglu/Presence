import UIKit

/// EXPERIMENT-only: SwiftUI's plain `App` protocol doesn't surface
/// `didFinishLaunchingWithOptions`, but we need it here to confirm whether
/// the OS is actually relaunching the app in the background for
/// CoreBluetooth state restoration (`.bluetoothCentrals` launch option) --
/// distinct from, and logged before, CentralConnectionMonitor.start() ever
/// runs. See CentralConnectionMonitor.swift.
final class AppDelegate: NSObject, UIApplicationDelegate {
    func application(
        _ application: UIApplication,
        didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]? = nil
    ) -> Bool {
        if launchOptions?[.bluetoothCentrals] != nil {
            CentralConnectionMonitor.staticLog("App launched by the OS for CoreBluetooth central restoration")
        } else {
            CentralConnectionMonitor.staticLog("App launched normally (no bluetoothCentrals launch option)")
        }
        return true
    }
}
