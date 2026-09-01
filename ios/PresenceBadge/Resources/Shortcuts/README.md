# Bundled shortcuts

`BadgeFocusOn.shortcut` and `BadgeFocusOff.shortcut` are here — each is a
"Set Focus → Do Not Disturb" shortcut (on/off respectively), hand-built in
the Shortcuts app and exported. `.shortcut` files are a signed binary
format only the Shortcuts app itself can produce, so if these ever need to
change, redo the steps below rather than editing the files directly.

## How to (re)create them (one-time, on your iPhone)

1. Open the **Shortcuts** app → **+** to create a new shortcut.
2. Add the **Set Focus** action.
3. Configure it: choose the Focus mode you want the badge to control, set
   it to turn **On**.
4. Name the shortcut exactly **`Badge Focus On`** (must match
   `FocusTrigger.onShortcutName` in `Sources/FocusTrigger.swift`).
5. Repeat for a second shortcut named **`Badge Focus Off`**, with **Set
   Focus** set to turn it **Off**.
6. For each shortcut: tap the shortcut's **⋯** menu → **Share** →
   **Export File** → save as a `.shortcut` file (AirDrop/Files to your Mac,
   or save directly to iCloud Drive and pull it from there).
7. Rename the exported files to `BadgeFocusOn.shortcut` /
   `BadgeFocusOff.shortcut` and drop them in this folder, replacing this
   README (or alongside it — xcodegen picks up all files under
   `Resources`).

Once both files are here, rerun `xcodegen generate` in `ios/PresenceBadge/`
so the project picks them up as bundle resources, then the onboarding
install buttons in the app will work.
