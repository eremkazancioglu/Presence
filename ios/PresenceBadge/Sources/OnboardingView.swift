import SwiftUI

struct OnboardingView: View {
    @Binding var onboardingComplete: Bool
    @State private var installedOn = false
    @State private var installedOff = false

    var body: some View {
        NavigationStack {
            VStack(alignment: .leading, spacing: 24) {
                Text("This app needs two Shortcuts installed once — Focus mode can only be toggled through Shortcuts' own \"Set Focus\" action, not directly by this app.")
                    .font(.body)

                VStack(alignment: .leading, spacing: 12) {
                    installButton(
                        title: "Install \"\(FocusTrigger.onShortcutName)\"",
                        resourceName: "BadgeFocusOn",
                        installed: $installedOn
                    )
                    installButton(
                        title: "Install \"\(FocusTrigger.offShortcutName)\"",
                        resourceName: "BadgeFocusOff",
                        installed: $installedOff
                    )
                }

                Text("Each opens Shortcuts to add it — tap \"Add Shortcut\" there, then come back here.")
                    .font(.footnote)
                    .foregroundStyle(.secondary)

                Spacer()

                Button("Done") {
                    onboardingComplete = true
                }
                .buttonStyle(.borderedProminent)
                .disabled(!(installedOn && installedOff))
                .frame(maxWidth: .infinity)
            }
            .padding()
            .navigationTitle("Setup")
        }
    }

    private func installButton(title: String, resourceName: String, installed: Binding<Bool>) -> some View {
        Button {
            ShortcutInstaller.shared.presentInstall(forResource: resourceName)
            installed.wrappedValue = true
        } label: {
            HStack {
                Text(title)
                Spacer()
                if installed.wrappedValue {
                    Image(systemName: "checkmark.circle.fill")
                        .foregroundStyle(.green)
                }
            }
        }
        .buttonStyle(.bordered)
    }
}
