import SwiftUI

@main
struct PresenceBadgeApp: App {
    @UIApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate
    @StateObject private var beaconMonitor = BeaconMonitor()
    @StateObject private var centralTestMonitor = CentralConnectionMonitor()
    @AppStorage("onboardingComplete") private var onboardingComplete = false
    @Environment(\.scenePhase) private var scenePhase

    init() {
        // Must run unconditionally at launch, not behind a button tap or
        // onAppear -- an OS-triggered background relaunch for CoreBluetooth
        // state restoration has no user interaction to wait for. See
        // CentralConnectionMonitor.start().
        _centralTestMonitor.wrappedValue.start()
    }

    var body: some Scene {
        WindowGroup {
            Group {
                if onboardingComplete {
                    ContentView(beaconMonitor: beaconMonitor, centralTestMonitor: centralTestMonitor)
                } else {
                    OnboardingView(onboardingComplete: $onboardingComplete)
                }
            }
            .onOpenURL { url in
                // Shortcuts hands control back here via x-success once the
                // Set Focus shortcut finishes (see FocusTrigger). Nothing to
                // do with it beyond returning focus to this app.
                print("Returned from Shortcuts: \(url)")
            }
            .onChange(of: onboardingComplete) { _, complete in
                if complete {
                    beaconMonitor.requestAuthorizationAndStartMonitoring()
                }
            }
            .onAppear {
                // Covers the case where onboarding was already completed
                // in a previous launch, so the onChange above never fires.
                if onboardingComplete {
                    beaconMonitor.requestAuthorizationAndStartMonitoring()
                }
            }
        }
        .onChange(of: scenePhase) { _, phase in
            guard onboardingComplete else { return }
            // Monitoring (background-reliable, no RSSI) runs continuously
            // regardless of scene phase. Ranging (live RSSI, for the
            // proximity filter and status UI) only makes sense in the
            // foreground.
            if phase == .active {
                beaconMonitor.startForegroundRanging()
            } else {
                beaconMonitor.stopForegroundRanging()
            }
        }
    }
}
