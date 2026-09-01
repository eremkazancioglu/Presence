import SwiftUI

struct ContentView: View {
    @ObservedObject var beaconMonitor: BeaconMonitor

    var body: some View {
        NavigationStack {
            Form {
                Section("Status") {
                    LabeledContent("Location permission", value: authorizationDescription)
                    LabeledContent("Precise location", value: beaconMonitor.accuracyAuthorization == .fullAccuracy ? "Yes" : "No (beacon detection won't work)")
                    LabeledContent("Foreground ranging", value: beaconMonitor.isRanging ? "Yes" : "No")
                    LabeledContent("Last badge state", value: beaconMonitor.badgeFocusState.map { $0 ? "ON" : "OFF" } ?? "—")
                    LabeledContent("Last RSSI", value: beaconMonitor.lastRSSI.map { "\($0) dBm" } ?? "—")
                    Text(beaconMonitor.lastEventDescription)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }

                Section {
                    Stepper(
                        "RSSI threshold: \(beaconMonitor.rssiThreshold) dBm",
                        value: $beaconMonitor.rssiThreshold,
                        in: -100...(-30),
                        step: 5
                    )
                } header: {
                    Text("Proximity filter")
                } footer: {
                    Text("Only applies while the app is in the foreground and actively ranging — background events (phone locked/backgrounded) have no signal-strength data to filter on. Calibrate against your actual badge.")
                }
            }
            .navigationTitle("Presence Badge")
        }
    }

    private var authorizationDescription: String {
        switch beaconMonitor.authorizationStatus {
        case .authorizedAlways: return "Always (required)"
        case .authorizedWhenInUse: return "When In Use (background won't work)"
        case .denied: return "Denied"
        case .restricted: return "Restricted"
        case .notDetermined: return "Not requested yet"
        @unknown default: return "Unknown"
        }
    }
}
