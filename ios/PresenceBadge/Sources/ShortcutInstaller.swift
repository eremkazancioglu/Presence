import UIKit

/// Presents a bundled .shortcut file so the system offers "Add Shortcut",
/// handing installation off to the Shortcuts app. Apple requires this
/// hand-off (and a first-run permission tap in Shortcuts) for any
/// automation coming from outside the Shortcuts app itself — there's no
/// way to install or grant run-permission fully silently.
final class ShortcutInstaller: NSObject, UIDocumentInteractionControllerDelegate {
    static let shared = ShortcutInstaller()

    private var interactionController: UIDocumentInteractionController?

    func presentInstall(forResource resourceName: String) {
        guard let url = Bundle.main.url(
            forResource: resourceName,
            withExtension: "shortcut",
            subdirectory: "Shortcuts"
        ) else {
            print("Missing bundled shortcut: \(resourceName).shortcut — see Resources/Shortcuts/README.md")
            return
        }

        guard let rootVC = UIApplication.shared.connectedScenes
            .compactMap({ ($0 as? UIWindowScene)?.keyWindow })
            .first?.rootViewController else { return }

        let controller = UIDocumentInteractionController(url: url)
        controller.delegate = self
        interactionController = controller
        controller.presentOpenInMenu(from: .zero, in: rootVC.view, animated: true)
    }
}
