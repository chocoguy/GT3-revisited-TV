# Releasing

A release is a git tag (`v1.0.0`) and a GitHub release with one file, **`GT3RevisitedInstaller.zip`**: the installer, signed with a Developer ID and notarized.

Players' installers see the new tag on GitHub, build the game from it on their Mac (`tools/tvos/port` from that tag), and reinstall it on every Apple TV it's installed on.

**The major version is the installer's.** The installer doesn't update itself:
- **`N.0.0` is a new installer.** The script builds, signs and notarizes it. Installers of an earlier major version show an alert that a new installer is available, link to the release, and stop updating the game until the player uses the new one.
- **`N.x.y` is a game update.** No new installer: the script attaches `vN.0.0`'s zip again, unchanged, so the latest release always has the installer to download. Installers of major `N` update the game to it.

An installer builds only the releases of its own major version. Everything it makes is in `~/Library/Application Support/GT3 Revisited Installer`, not in the app, so replacing the app with a new download changes nothing else.

`tools/release.sh` does all of it on your Mac. Nothing signs or publishes in CI (`.github/workflows/build.yml` only builds and tests).

## One-time setup

1. **Developer ID certificate.** Only the team's Account Holder can make one.
   - In Xcode → Settings → Accounts, pick team `USLHQ524FW` and choose Manage Certificates… → + → **Developer ID Application**.
   - Or create it on developer.apple.com → Certificates, with a request (CSR) made in this Mac's Keychain Access, so the private key is on this Mac.
   - Check it's in the keychain: `security find-identity -v -p codesigning` lists `Developer ID Application: … (USLHQ524FW)`.
   - Keychain error -25294 on import means the default keychain is unusable. Import with `security import <cer> -k ~/Library/Keychains/login.keychain-db`.
2. **Notarization credentials:** the keychain profile `frontline-notary`, shared with Frontline Rewritten (the same team and Apple ID). If it's missing on this Mac, make it again:
   ```sh
   xcrun notarytool store-credentials frontline-notary --apple-id <your Apple ID> --team-id USLHQ524FW
   ```
   It asks for an app-specific password: make one at account.apple.com → Sign-In and Security → App-Specific Passwords.
3. **GitHub CLI:**
   ```sh
   brew install gh
   ```
   ```sh
   gh auth login
   ```

## Each release

From a clean `master` that's pushed:

```sh
tools/release.sh 1.0.0
```

It does this, in order:
1. Checks the above, and that the tag is new here and on GitHub. For a game update (`1.2.0`), checks `v1.0.0` on GitHub has the installer.
2. Opens your editor for the release notes (Markdown), or takes `--notes <file>`.
3. Sets `port.json`'s `version` (and `minAppVersion` to `N.0.0`: 1.0.x installers still read it), commits "Release 1.0.0", and tags `v1.0.0`.
4. **New major version only:** builds GT3 Revisited Installer (Release), stamped 1.0.0 (build number 10000), signs it (hardened runtime, timestamp), notarizes the zip, staples the app, checks it with `spctl`, and zips it again into `build/dist/1.0.0/`.
   **Game update:** downloads `vN.0.0`'s `GT3RevisitedInstaller.zip` into `build/dist/<version>/` instead.
5. Asks before pushing, then pushes `master` and the tag, and creates the GitHub release with the zip.

If you answer no at the push, the commit and tag stay local. The script prints how to undo them.

**Options:**
- **Release candidates:** `tools/release.sh 1.0.0-rc1 --prerelease`. GitHub marks it a pre-release, so players don't see it (their installers skip pre-releases). To test one, turn on pre-releases in your installer:
  ```sh
  defaults write com.chocoguy.gt3r.installer includePrereleases -bool YES
  ```
- **A release that needs a new installer:** give it the next major version (`2.0.0`).
- **Dry run:** `--dry-run` runs the checks (reporting, not stopping) and builds an ad hoc installer into `build/dist`. It signs, notarizes, commits, tags and pushes nothing.

## Versions

- This port's tags are `v` plus a semantic version, from `v1.0.0`. The upstream game's tags (`v0.x`) come into a checkout that has an `upstream` remote, so the port's versions start above them. `git config remote.upstream.tagOpt --no-tags` keeps them out.
- The installer's version is the `N.0.0` release it was built for; the game's is the release's. The Apple TV app's `CFBundleShortVersionString` is its numeric part, and `CFBundleVersion` is `major×10000 + minor×100 + patch`.
- `GT3RPortVersion` (the full version, `-rc1` included) and `GT3RPortCommit` are in the app's Info.plist; `GT3RInstallerVersion` and `GT3RInstallerCommit` are in the installer's.
- The game's own version (`project(GT3Revisited VERSION …)` in `CMakeLists.txt`, the upstream engine's) stays as it is: online play compares it with the upstream relay's. Change it only when merging a newer upstream.
