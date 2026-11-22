# Releasing

A release is a git tag (`v1.0.0`) and a GitHub release with two files:
- **`GT3RevisitedInstaller-<version>.zip`:** the installer, signed with a Developer ID and notarized.
- **`appcast.xml`:** Sparkle's feed. Installers read it from `releases/latest/download/appcast.xml` to update themselves.

Players' installers see the new tag on GitHub, build the game from it on their Mac (`tools/tvos/port` from that tag), and reinstall it on every Apple TV it's installed on.

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
3. **Sparkle's signing key:** the same EdDSA key pair as Frontline Rewritten. `apps/installer/sparkle-public-key.txt` holds its public key, and the private key is in your login keychain ("Private key for signing Sparkle updates"), where `generate_appcast` finds it. Keep a backup (`generate_keys -x <file>`): **without it, neither project's installers can take updates.** To check the key in the keychain matches, after building the installer once:
   ```sh
   build-installer/_deps/sparkle-src/bin/generate_keys -p
   ```
   It prints the same key as `apps/installer/sparkle-public-key.txt`.
4. **GitHub CLI:**
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
1. Checks the above, and that the tag is new here and on GitHub.
2. Opens your editor for the release notes (Markdown), or takes `--notes <file>`.
3. Sets `port.json`'s `version`, commits "Release 1.0.0", and tags `v1.0.0`.
4. Builds GT3 Revisited Installer (Release), stamped 1.0.0 (build number 10000), with Sparkle's public key.
5. Signs Sparkle's parts inside-out, then the app (hardened runtime, timestamp).
6. Notarizes the zip, staples the app, checks it with `spctl`, and zips it again into `build/dist/1.0.0/`.
7. Makes `appcast.xml` with Sparkle's `generate_appcast` (signed with your key, release notes embedded).
8. Asks before pushing, then pushes `master` and the tag, and creates the GitHub release with both files.

If you answer no at the push, the commit and tag stay local. The script prints how to undo them.

**Options:**
- **Release candidates:** `tools/release.sh 1.0.0-rc1 --prerelease`. GitHub marks it a pre-release, so players don't see it (their installers skip pre-releases, and `latest/download` skips them for Sparkle). To test one, turn on pre-releases in your installer:
  ```sh
  defaults write com.chocoguy.gt3r.installer includePrereleases -bool YES
  ```
- **A release that needs a newer installer:** `--min-app 1.1.0` sets `port.json`'s `minAppVersion`. Older installers then stop before building it and ask to update themselves first.
- **Dry run:** `--dry-run` runs the checks (reporting, not stopping) and builds an ad hoc installer into `build/dist`. It signs, notarizes, commits, tags and pushes nothing.

## Versions

- This port's tags are `v` plus a semantic version, from `v1.0.0`. The upstream game's tags (`v0.x`) come into a checkout that has an `upstream` remote, so the port's versions start above them. `git config remote.upstream.tagOpt --no-tags` keeps them out.
- The installer and the game share the release's version. The Apple TV app's `CFBundleShortVersionString` is its numeric part, and `CFBundleVersion` is `major×10000 + minor×100 + patch`.
- `GT3RPortVersion` (the full version, `-rc1` included) and `GT3RPortCommit` are in the app's Info.plist; `GT3RInstallerVersion` and `GT3RInstallerCommit` are in the installer's.
- The game's own version (`project(GT3Revisited VERSION …)` in `CMakeLists.txt`, the upstream engine's) stays as it is: online play compares it with the upstream relay's. Change it only when merging a newer upstream.
