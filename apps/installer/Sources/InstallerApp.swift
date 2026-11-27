// GT3 Revisited Installer: builds GT3 Revisited from the player's own disc image, at a GitHub release
// tag, and installs it on their Apple TVs; updates them all when a new release comes out.
// Distributed signed and notarized (tools/release.sh). It doesn't update itself: a new major version
// of the game is a new installer, which the player downloads; until then it says so, and game updates wait.

import SwiftUI

@main
struct InstallerApp: App {
    @StateObject private var model = AppModel()

    var body: some Scene {
        Window("GT3 Revisited Installer", id: "main") {
            ContentView()
                .environmentObject(model)
                .task { model.start() }
        }
    }
}
