import SwiftUI

struct ContentView: View {
    @ObservedObject var scanner: BadgeScanner

    var body: some View {
        NavigationStack {
            Form {
                Section("Status") {
                    LabeledContent("Scanning", value: scanner.isScanning ? "Yes" : "No")
                    LabeledContent("Last badge state", value: scanner.badgeFocusState.map { $0 ? "ON" : "OFF" } ?? "—")
                    LabeledContent("Last RSSI", value: scanner.lastRSSI.map { "\($0) dBm" } ?? "—")
                    Text(scanner.lastEventDescription)
                        .font(.footnote)
                        .foregroundStyle(.secondary)
                }

                Section {
                    Stepper(
                        "RSSI threshold: \(scanner.rssiThreshold) dBm",
                        value: $scanner.rssiThreshold,
                        in: -100...(-30),
                        step: 5
                    )
                } header: {
                    Text("Proximity filter")
                } footer: {
                    Text("Only badge sightings at or above this signal strength trigger Focus. Higher (closer to 0) means the badge must be closer. Calibrate against your actual badge.")
                }
            }
            .navigationTitle("Presence Badge")
        }
    }
}
