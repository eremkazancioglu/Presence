import SwiftUI

@main
struct PresenceBadgeApp: App {
    @StateObject private var scanner = BadgeScanner()
    @AppStorage("onboardingComplete") private var onboardingComplete = false

    var body: some Scene {
        WindowGroup {
            if onboardingComplete {
                ContentView(scanner: scanner)
            } else {
                OnboardingView(onboardingComplete: $onboardingComplete)
            }
        }
    }
}
