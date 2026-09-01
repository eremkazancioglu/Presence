import UIKit

/// Triggers Focus mode by running a pre-built Shortcut, since third-party
/// apps have no direct API to toggle system Focus — the "Set Focus"
/// action is only available inside Shortcuts itself. See the onboarding
/// flow (OnboardingView) for how the two shortcuts get installed.
struct FocusTrigger {
    static let onShortcutName = "Badge Focus On"
    static let offShortcutName = "Badge Focus Off"

    func setFocus(on: Bool) {
        let name = on ? Self.onShortcutName : Self.offShortcutName
        guard let encodedName = name.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed),
              let url = URL(string: "shortcuts://run-shortcut?name=\(encodedName)") else { return }

        DispatchQueue.main.async {
            UIApplication.shared.open(url)
        }
    }
}
