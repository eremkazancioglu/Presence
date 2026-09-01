import UIKit

/// Triggers Focus mode by running a pre-built Shortcut, since third-party
/// apps have no direct API to toggle system Focus — the "Set Focus"
/// action is only available inside Shortcuts itself. See the onboarding
/// flow (OnboardingView) for how the two shortcuts get installed.
///
/// Uses the x-callback-url pattern so Shortcuts hands control straight
/// back to this app once the shortcut finishes, instead of leaving the
/// user sitting in the Shortcuts app. This still requires each shortcut
/// to have "Ask Before Running" turned off (Shortcuts app -> shortcut's
/// info/details) — otherwise iOS stops to show a confirmation dialog
/// regardless of the URL scheme used.
struct FocusTrigger {
    static let onShortcutName = "Badge Focus On"
    static let offShortcutName = "Badge Focus Off"

    /// Must match the CFBundleURLSchemes entry in project.yml.
    private static let callbackScheme = "presencebadge"

    func setFocus(on: Bool) {
        let name = on ? Self.onShortcutName : Self.offShortcutName
        guard let encodedName = name.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) else { return }

        let successURL = "\(Self.callbackScheme)://focus-set?state=\(on ? "on" : "off")"
        guard let encodedSuccess = successURL.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed) else { return }

        guard let url = URL(string: "shortcuts://x-callback-url/run-shortcut?name=\(encodedName)&x-success=\(encodedSuccess)") else { return }

        DispatchQueue.main.async {
            UIApplication.shared.open(url)
        }
    }
}
