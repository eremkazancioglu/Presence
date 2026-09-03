import SwiftUI

struct ContentView: View {
    @ObservedObject var beaconMonitor: BeaconMonitor
    @ObservedObject var centralTestMonitor: CentralConnectionMonitor
    @State private var centralTestLog = ""

    var body: some View {
        NavigationStack {
            Form {
                Section {
                    Text(centralTestMonitor.statusDescription)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                    Button("Start background-open() test") {
                        centralTestMonitor.start()
                    }
                    Button("Refresh log") {
                        centralTestLog = centralTestMonitor.readLog()
                    }
                    Button("Clear log", role: .destructive) {
                        centralTestMonitor.clearLog()
                        centralTestLog = ""
                    }
                    if !centralTestLog.isEmpty {
                        ShareLink(item: centralTestLog) {
                            Label("Share log", systemImage: "square.and.arrow.up")
                        }
                        ScrollView {
                            Text(centralTestLog)
                                .font(.system(.footnote, design: .monospaced))
                                .frame(maxWidth: .infinity, alignment: .leading)
                                .textSelection(.enabled)
                        }
                        .frame(maxHeight: 300)
                    }
                } header: {
                    Text("Experiment: CoreBluetooth background open()")
                } footer: {
                    Text("Tests whether UIApplication.open() works from a CBCentralManager didConnect/didDisconnect callback while locked. Flash firmware/experiments/central_bg_test/ to the badge first, tap Start, then lock the phone and press the badge's button. Tap Refresh log after to see results.")
                }

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
