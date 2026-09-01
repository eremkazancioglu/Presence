import SwiftUI

@main
struct PresenceBadgeApp: App {
    @StateObject private var scanner = BadgeScanner()
    @AppStorage("onboardingComplete") private var onboardingComplete = false

    var body: some Scene {
        WindowGroup {
            Group {
                if onboardingComplete {
                    ContentView(scanner: scanner)
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
        }
    }
}
