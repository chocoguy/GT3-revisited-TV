# GT3 Revisited - tvOS Port

![Logo](https://mangogravy.com/gt3-small.jpg)

This is a fork of matt-pulsipher's [GT3 Recomp](https://github.com/matt-pulsipher/GT3-revisited) all credit goes to him.

This is an open-source reimplementation of the *Gran Turismo 3 A-Spec* engine for tvOS

GT3 Revisited contains **no game data or assets**. It needs a
disc image (`.iso` or `.chd`) that you made from your own copy of the game.

## Requirements

- An Apple Silicon Mac
- macOS 27
- XCode 27
- An Apple TV 4K (3rd gen or newer) with tvOS 27, paired with Xcode (**Xcode → Window → Devices and Simulators**).
- An Apple ID signed in to Xcode (**Xcode → Settings → Accounts**). A free one works, but its installs open for 7 days; then reinstall to renew. A paid Apple Developer account lasts a year.
- Gamepad


## Install

1. Download **GT3 Revisited Installer** from the [latest release](https://github.com/chocoguy/GT3-revisited-TV/releases/latest) and ***move it to your Applications folder.*** It is signed and notarized, so it opens with a double-click.
2. Did I mention to move the .app package to the ***Applications Folder?***
3. Open it and go to **Setup**. It checks Xcode, its tvOS platform and your Apple ID, and each problem it finds comes with a button that fixes it. It downloads anything else it needs itself, so you never have to open the Terminal.
4. Pair your Apple TV with Xcode if you haven't already. On the TV, open **Settings → Remotes and Devices → Remote App and Devices**; on the Mac, open **Xcode → Window → Devices and Simulators**. Then turn on **Developer Mode** on the TV (**Settings → Privacy & Security**).
5. On the **Game** page, choose your disc image (`.iso` or `.chd`), then click **Install On** next to your Apple TV. The first build takes a while. The first install sends the whole game, disc included, to the TV, which takes about 5 minutes over the network.
6. Start **GT3 Revisited** from the Apple TV's Home screen. The installer never starts it for you.

**Updates:** the **Updates** page shows new releases and their notes. **Update All** builds the new version and installs it on every Apple TV you installed on before. When a release needs a new installer, it says so and links to it: download it and open it instead of the old one. Your disc, Apple TVs and build carry over.

**Free Apple ID:** the game stops opening 7 days after it was installed. The installer shows the date. Click **Reinstall** before then to renew it.

Your disc image and the app built from it stay on your own Mac and Apple TV. Everything the installer makes (the source, the Xcode build, CMake, logs) is in one folder, `~/Library/Application Support/GT3 Revisited Installer`: **Setup → Storage** shows its size, frees the build (several GB) or removes it all.

## Controls

Play with a game controller (a DualSense, DualShock 4, Xbox, Switch Pro or other MFi controller), paired with the Apple TV under **Settings → Remotes and Devices → Bluetooth**. The game doesn't read the Siri Remote: its Home button still takes you back to the Home screen.

The buttons match the original's DualShock 2 layout. In races, the default (**modern**) layout is:

| Action | Controller |
|---|---|
| Steer | Left stick / D-pad |
| Accelerate / brake | Right trigger / left trigger |
| Shift up / down | Cross / Square (A / X), or the back paddles |
| Handbrake | Circle (B) |
| Reverse | L1 (held) |
| Change view | Triangle (Y) |
| Look back | R3 (held) |
| Look around | Right stick |
| Pause | Start |
| Show / hide the ghost (time trials, licences) | Select |

To use the original's layout, open **Settings → Controls → Race controls** and choose **gt3**. In menus, Cross confirms and Triangle goes back. You can remap any button under **Settings → Controls**. The full reference is [docs/INPUT.md](docs/INPUT.md).

## For Developers

Building by hand needs the same Mac, Xcode and paired Apple TV as above, plus CMake 3.21 or later. Sign with your own team ID and a bundle identifier of your own:

```bash
cmake -S . -B build-tvos -G Xcode -DCMAKE_SYSTEM_NAME=tvOS -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
      -DGT3R_IOS_TEAM=<team ID> -DGT3R_IOS_BUNDLE_ID=<your.own.identifier>
cmake --build build-tvos --config RelWithDebInfo --target gt3r -- -allowProvisioningUpdates
```

`tools/tvos/tv.py` wraps Xcode's `devicectl` for working with the TV:

```bash
tools/tvos/tv.py devices                  # the paired Apple TVs
tools/tvos/tv.py build --disc <image>     # build with your own disc image inside the app
tools/tvos/tv.py install                  # install the newest build
tools/tvos/tv.py launch                   # start it
tools/tvos/tv.py log                      # the game's log
```

`tools/tvos/port` is the build engine the installer runs. It takes the same steps (`check`, `prepare`, `build`, `install`, `installed`), and with `--json` prints one event per line. The installer's source is in `apps/installer`.

Unit tests run on the Mac:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target gt3r_tests && ctest --test-dir build
```

- [docs/dev/PLATFORMS.md](docs/dev/PLATFORMS.md) ("tvOS"): how the Apple TV build works, and what was measured on a device
- [docs/release.md](docs/release.md): publishing a release with `tools/release.sh`
- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) and [CONTRIBUTING.md](CONTRIBUTING.md): the engine, and how to contribute

## Credits

Original game © Polyphony, Sony

Shoutouts to matt-pulsipher and Claude