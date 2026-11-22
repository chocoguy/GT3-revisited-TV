#!/usr/bin/env python3
"""Installs, starts and reads back GT3 Revisited on an Apple TV (docs/dev/PLATFORMS.md "tvOS").

An Apple TV has no cable port and no Files app: everything goes over the network through
Xcode's devicectl, with the TV paired in Xcode (Window > Devices and Simulators) and in
developer mode. Needs only Python 3 and Xcode.

  tools/tvos/tv.py devices                     the Apple TVs this Mac is paired with
  tools/tvos/tv.py build [--disc <image>]      configure and build the signed app, with a disc
                                               image of your own inside it or (--no-disc) without
  tools/tvos/tv.py install                     install the built app
  tools/tvos/tv.py launch [-e NAME=value] [-- game arguments]
                                               start the game (a running one is ended first)
  tools/tvos/tv.py push-disc <image>           send a disc image (.iso, .chd) to the game's folder
  tools/tvos/tv.py log [--prev]                the game's log of this run (or the run before)
  tools/tvos/tv.py reports                     why the last run ended: the system's reports and
                                               the log's last lines about memory and heat
  tools/tvos/tv.py saves pull [folder]         copy the settings, saves and replays to the Mac
  tools/tvos/tv.py saves push <folder>         put a copy back (the game must not be running)

The app is build-tvos/<config>-appletvos/gt3r.app, the newest one, unless --app names another;
its Info.plist gives the bundle identifier (or --bundle-id). --device takes a TV's name or
identifier when more than one is paired.
"""
import argparse
import glob
import json
import os
import plistlib
import subprocess
import sys
import tempfile
import time

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
# The user data folder inside the app's container (game::user_data_dir: SDL_GetPrefPath, which
# on tvOS is under Library/Caches, the only folder an app can write).
DATA_DIR = "Library/Caches/GT3Revisited/GT3Revisited"


class Failed(Exception):
    pass


def devicectl(*args, want_json=False, check=True):
    """Runs `xcrun devicectl <args>`. With want_json, returns the "result" of its JSON output."""
    cmd = ["xcrun", "devicectl", *args]
    out_path = None
    if want_json:
        fd, out_path = tempfile.mkstemp(suffix=".json")
        os.close(fd)
        cmd += ["--json-output", out_path]
    try:
        run = subprocess.run(cmd, capture_output=True, text=True)
        if check and run.returncode != 0:
            raise Failed((run.stderr or run.stdout).strip() or "devicectl %s failed" % " ".join(args[:3]))
        if not want_json:
            return run
        try:
            with open(out_path) as f:
                return json.load(f).get("result", {})
        except ValueError: # a failed command (check=False) may leave no JSON
            return {}
    finally:
        if out_path and os.path.exists(out_path):
            os.remove(out_path)


def apple_tvs():
    """The real Apple TVs devicectl knows: [{name, id, state, model}]."""
    tvs = []
    for d in devicectl("list", "devices", want_json=True).get("devices", []):
        hw, props, conn = d.get("hardwareProperties", {}), d.get("deviceProperties", {}), d.get("connectionProperties", {})
        if hw.get("platform") != "tvOS" or hw.get("reality") != "physical":
            continue
        tvs.append({
            "name": props.get("name", "?"),
            "id": hw.get("udid") or d.get("identifier", ""),
            "state": "%s, %s" % (conn.get("pairingState", "?"), conn.get("tunnelState", "?")),
            "model": hw.get("marketingName") or hw.get("productType", ""),
        })
    return tvs


def pick_tv(wanted):
    tvs = apple_tvs()
    if wanted:
        for tv in tvs:
            if wanted in (tv["name"], tv["id"]):
                return tv
        raise Failed("No paired Apple TV is called %r. `tv.py devices` lists them." % wanted)
    if not tvs:
        raise Failed("No Apple TV is paired with this Mac. Pair one in Xcode: Window > Devices and Simulators.")
    if len(tvs) > 1:
        raise Failed("More than one Apple TV is paired; choose one with --device:\n" +
                     "\n".join("  %s  (%s)" % (tv["name"], tv["id"]) for tv in tvs))
    return tvs[0]


def find_app(given):
    if given:
        if not os.path.isdir(given):
            raise Failed("%s is not an app" % given)
        return given
    apps = glob.glob(os.path.join(ROOT, "build-tvos", "*-appletvos", "gt3r.app"))
    if not apps:
        raise Failed("No built app in build-tvos/. Build it first (docs/dev/PLATFORMS.md \"tvOS\"), or pass --app.")
    return max(apps, key=os.path.getmtime)


def bundle_id(args):
    if args.bundle_id:
        return args.bundle_id
    with open(os.path.join(find_app(args.app), "Info.plist"), "rb") as f:
        return plistlib.load(f)["CFBundleIdentifier"]


def cmd_devices(args):
    tvs = apple_tvs()
    if not tvs:
        print("No Apple TV is paired with this Mac.")
        return
    for tv in tvs:
        print("%-24s %-28s %s  %s" % (tv["name"], tv["state"], tv["id"], tv["model"]))


def cached(name):
    """A value of build-tvos/CMakeCache.txt ("" when it is not there)."""
    try:
        with open(os.path.join(ROOT, "build-tvos", "CMakeCache.txt")) as f:
            for line in f:
                if line.startswith(name + ":"):
                    return line.split("=", 1)[1].strip()
    except OSError:
        pass
    return ""


def profile_expiry(app):
    """When the app's provisioning profile ends (the app stops opening then), or None."""
    run = subprocess.run(["security", "cms", "-D", "-i", os.path.join(app, "embedded.mobileprovision")],
                         capture_output=True)
    try:
        return plistlib.loads(run.stdout)["ExpirationDate"]
    except Exception:
        return None


def cmd_build(args):
    team = args.team or cached("GT3R_IOS_TEAM")
    bundle = args.bundle_id or cached("GT3R_IOS_BUNDLE_ID")
    if not team or not bundle:
        raise Failed("The first build needs --team <your Apple team ID> and --bundle-id <an identifier of your "
                     "own, e.g. com.you.gt3r> (docs/dev/PLATFORMS.md \"iOS\" says where the team ID is).")
    # The disc image built into the app: as given, none with --no-disc, else as the build tree has it.
    disc = "" if args.no_disc else os.path.abspath(args.disc) if args.disc else cached("GT3R_TVOS_DISC")
    if disc and not os.path.isfile(disc):
        raise Failed("%s is not a file" % disc)
    build = os.path.join(ROOT, "build-tvos")

    def run(cmd):
        if subprocess.run(cmd, cwd=ROOT).returncode != 0:
            raise Failed("`%s` failed" % " ".join(cmd[:4]))

    run(["cmake", "-S", ROOT, "-B", build, "-G", "Xcode", "-DCMAKE_SYSTEM_NAME=tvOS",
         "-DCMAKE_OSX_DEPLOYMENT_TARGET=16.0", "-DGT3R_IOS_TEAM=" + team, "-DGT3R_IOS_BUNDLE_ID=" + bundle,
         "-DGT3R_TVOS_DISC=" + disc])
    cmd = ["cmake", "--build", build, "--config", args.config, "--target", "gt3r", "--", "-quiet",
           "-allowProvisioningUpdates"]
    run(cmd)
    app = os.path.join(build, args.config + "-appletvos", "gt3r.app")
    size = sum(os.path.getsize(os.path.join(d, f)) for d, _, files in os.walk(app) for f in files)
    print("Built %s (%.2f GB), %s." % (os.path.relpath(app, ROOT), size / 1e9,
                                        "with " + os.path.basename(disc) if disc else "without a disc image"))
    if expiry := profile_expiry(app):
        print("It opens until %s (UTC); building and installing again renews it." % expiry.strftime("%Y-%m-%d %H:%M"))


def cmd_install(args):
    tv, app = pick_tv(args.device), find_app(args.app)
    if not os.path.isdir(os.path.join(app, "_CodeSignature")):
        raise Failed("%s is not signed, and an Apple TV installs only signed apps: configure with "
                     "-DGT3R_IOS_TEAM and -DGT3R_IOS_BUNDLE_ID." % app)
    print("Installing %s on %s..." % (os.path.relpath(app, ROOT), tv["name"]))
    start = time.time()
    devicectl("device", "install", "app", "--device", tv["id"], app)
    print("Installed in %.0f s." % (time.time() - start))


def running_pids(tv, app_name="gt3r.app"):
    procs = devicectl("device", "info", "processes", "--device", tv["id"], want_json=True).get("runningProcesses", [])
    return [p["processIdentifier"] for p in procs
            if ("/%s/" % app_name) in p.get("executable", "") and "processIdentifier" in p]


def cmd_launch(args):
    tv, bundle = pick_tv(args.device), bundle_id(args)
    env = {}
    for pair in args.env:
        name, eq, value = pair.partition("=")
        if not eq or not name:
            raise Failed("-e takes NAME=value (got %r)" % pair)
        env[name] = value
    # A running game is ended first, and given a moment: started over it, the new one can be
    # covered by the Home screen coming back from the old one, and tvOS then suspends it.
    pids = running_pids(tv)
    for pid in pids:
        devicectl("device", "process", "terminate", "--device", tv["id"], "--pid", str(pid), check=False)
    if pids:
        time.sleep(1.5)
    cmd = ["device", "process", "launch", "--device", tv["id"]]
    if env:
        cmd += ["--environment-variables", json.dumps(env)]
    game_args = args.game_args[1:] if args.game_args[:1] == ["--"] else args.game_args
    run = devicectl(*cmd, bundle, *game_args, check=False)
    if run.returncode != 0:
        raise Failed((run.stderr or run.stdout).strip() +
                     "\nThe game did not start. An Apple TV that is asleep installs apps but starts none: "
                     "wake it and try again.")
    print("Started %s on %s." % (bundle, tv["name"]))


def remote_files(tv, bundle, subdirectory=DATA_DIR):
    """The files of a folder in the app's container: {name: size} (empty when it is not there)."""
    run = devicectl("device", "info", "files", "--device", tv["id"], "--domain-type", "appDataContainer",
                    "--domain-identifier", bundle, "--subdirectory", subdirectory, want_json=True, check=False)
    return {f["name"]: f.get("metadata", {}).get("size", -1) for f in run.get("files", [])
            if not f.get("resources", {}).get("isDirectory")}


def cmd_push_disc(args):
    tv, bundle = pick_tv(args.device), bundle_id(args)
    if not os.path.isfile(args.image):
        raise Failed("%s is not a file" % args.image)
    if not args.image.lower().endswith((".iso", ".chd")):
        raise Failed("A disc image is a .iso or a .chd")
    name, size = os.path.basename(args.image), os.path.getsize(args.image)
    # The image is large and the game's folder is one tvOS may empty: sent again only when it is
    # not there whole.
    if remote_files(tv, bundle).get(name) == size:
        print("%s is already on %s (%.2f GB)." % (name, tv["name"], size / 1e9))
        return
    print("Sending %s (%.2f GB) to %s..." % (name, size / 1e9, tv["name"]))
    start = time.time()
    devicectl("device", "copy", "to", "--device", tv["id"], "--domain-type", "appDataContainer",
              "--domain-identifier", bundle, "--source", args.image, "--destination", "%s/%s" % (DATA_DIR, name))
    seconds = max(time.time() - start, 0.001)
    if remote_files(tv, bundle).get(name) != size:
        raise Failed("%s did not arrive whole on %s" % (name, tv["name"]))
    print("Sent in %.0f s (%.0f MB/s). The game opens it at its next start." % (seconds, size / 1e6 / seconds))


# What `saves` copies, inside the user data folder (as platform/tv_persist.h keeps on the TV).
KEPT_FILES = ["config.ini", "music_playlist.txt"]
KEPT_FOLDERS = ["saves", "replays"]


def remote_tree(tv, bundle):
    """The kept files on the TV: {path inside the user data folder: (size, modified)}."""
    def listing(sub):
        run = devicectl("device", "info", "files", "--device", tv["id"], "--domain-type", "appDataContainer",
                        "--domain-identifier", bundle, "--subdirectory", DATA_DIR + ("/" + sub if sub else ""),
                        want_json=True, check=False)
        return run.get("files", [])

    tree = {}

    def walk(sub, depth=0):
        for f in listing(sub):
            path = sub + "/" + f["name"]
            # (a listing may or may not go into folders itself: only its own level is taken)
            if "/" in f.get("relativePath", f["name"]).strip("/"):
                continue
            if f.get("resources", {}).get("isDirectory"):
                if depth < 4:
                    walk(path, depth + 1)
            elif not f["name"].endswith(".tmp"):
                meta = f.get("metadata", {})
                tree[path] = (meta.get("size"), meta.get("lastModDate"))

    top = {f["name"]: f for f in listing("")}
    for name in KEPT_FILES:
        if name in top:
            meta = top[name].get("metadata", {})
            tree[name] = (meta.get("size"), meta.get("lastModDate"))
    for name in KEPT_FOLDERS:
        if name in top:
            walk(name)
    return tree


def cmd_saves(args):
    tv, bundle = pick_tv(args.device), bundle_id(args)
    if args.action == "pull":
        folder = args.folder or os.path.join(os.path.expanduser("~"), "Documents", "GT3 Revisited TV backup",
                                              time.strftime("%Y-%m-%d %H.%M.%S"))
        before = remote_tree(tv, bundle)
        if not before:
            raise Failed("No settings or saves on %s (%s)." % (tv["name"], bundle))
        with tempfile.TemporaryDirectory() as tmp:
            for path in sorted(before):
                local = os.path.join(tmp, *path.split("/"))
                os.makedirs(os.path.dirname(local), exist_ok=True)
                devicectl("device", "copy", "from", "--device", tv["id"], "--domain-type", "appDataContainer",
                          "--domain-identifier", bundle, "--source", "%s/%s" % (DATA_DIR, path), "--destination", local)
            # The game saves by writing a file and renaming it: a copy taken while it did so is
            # not kept.
            if remote_tree(tv, bundle) != before:
                raise Failed("The game wrote its saves while they were copied; nothing was kept. Try again.")
            os.makedirs(os.path.dirname(folder), exist_ok=True)
            if os.path.exists(folder):
                raise Failed("%s is already there" % folder)
            os.rename(tmp, folder)
            os.makedirs(tmp) # (for TemporaryDirectory to remove)
        print("%d files copied to %s" % (len(before), folder))
        return
    if not args.folder or not os.path.isdir(args.folder):
        raise Failed("saves push takes the folder of a copy made with saves pull")
    if running_pids(tv):
        raise Failed("The game is running on %s; close it first (it would write over the copy)." % tv["name"])
    files = [n for n in KEPT_FILES if os.path.isfile(os.path.join(args.folder, n))]
    for top in KEPT_FOLDERS:
        for d, _, names in os.walk(os.path.join(args.folder, top)):
            files += [os.path.relpath(os.path.join(d, n), args.folder).replace(os.sep, "/") for n in names]
    if not files:
        raise Failed("%s has no settings, saves or replays" % args.folder)
    for path in sorted(files):
        devicectl("device", "copy", "to", "--device", tv["id"], "--domain-type", "appDataContainer",
                  "--domain-identifier", bundle, "--source", os.path.join(args.folder, *path.split("/")),
                  "--destination", "%s/%s" % (DATA_DIR, path))
    print("%d files put on %s" % (len(files), tv["name"]))


def fetch_log(tv, bundle, name):
    """The text of a log file of the game ("" when it is not there)."""
    with tempfile.TemporaryDirectory() as tmp:
        local = os.path.join(tmp, name)
        devicectl("device", "copy", "from", "--device", tv["id"], "--domain-type", "appDataContainer",
                  "--domain-identifier", bundle, "--source", "%s/%s" % (DATA_DIR, name), "--destination", local,
                  check=False)
        if not os.path.exists(local):
            return ""
        with open(local, errors="replace") as f:
            return f.read()


def read_ips(path):
    """A system report: its first line (a JSON header) and its body (JSON, or text for old ones)."""
    with open(path, errors="replace") as f:
        first, _, rest = f.read().partition("\n")
    try:
        header = json.loads(first)
    except ValueError:
        header = {}
    try:
        body = json.loads(rest)
    except ValueError:
        body = None
    return header, body


def cmd_reports(args):
    tv, bundle = pick_tv(args.device), bundle_id(args)
    # tvOS ends an app that passes its memory limit with no crash report: at most a JetsamEvent
    # report names it, and not every time. The log's last lines then say how close the run was.
    found = []
    with tempfile.TemporaryDirectory() as tmp:
        run = devicectl("device", "copy", "from", "--device", tv["id"], "--domain-type", "systemCrashLogs",
                        "--source", ".", "--destination", tmp, check=False)
        if run.returncode != 0:
            print("The system's reports could not be read: %s" % (run.stderr or run.stdout).strip().splitlines()[-1:])
        for d, _, names in os.walk(tmp):
            for n in names:
                if not n.endswith(".ips"):
                    continue
                header, body = read_ips(os.path.join(d, n))
                when = header.get("timestamp", "")
                if n.startswith("JetsamEvent") and isinstance(body, dict):
                    page = body.get("memoryStatus", {}).get("pageSize", 16384)
                    for p in body.get("processes", []):
                        if p.get("name") == "gt3r" and p.get("reason"):
                            found.append((when, "%s  ended by the system: %s, %.0f MB" %
                                          (when, p["reason"], p.get("rpages", 0) * page / 1e6)))
                elif n.startswith("gt3r") or header.get("app_name") == "gt3r" or header.get("name") == "gt3r":
                    what = ""
                    if isinstance(body, dict):
                        exc = body.get("exception", {})
                        what = " ".join(str(exc.get(k, "")) for k in ("type", "signal", "subtype")).strip()
                    found.append((when, "%s  crashed%s  (%s)" % (when, ": " + what if what else "", n)))
    found.sort()
    print("System reports naming the game on %s:" % tv["name"])
    for _, line in found[-args.count:]:
        print("  " + line)
    if not found:
        print("  none")
    for name in ("log.prev.txt", "log.txt"):
        lines = [l for l in fetch_log(tv, bundle, name).splitlines()
                 if "Stats:" in l or "Thermal state" in l or " E " in l[:14] or " W " in l[:14]]
        print("\n%s, its last lines about memory, heat and trouble:" % name)
        for l in lines[-8:]:
            print("  " + l[:200])
        if not lines:
            print("  none (or no such log)")
    if not found:
        print("\nA game that closed by itself with no report was most likely out of memory: not every such end "
              "is reported, and a jump can fall between two lines of the log.")


def cmd_log(args):
    tv, bundle = pick_tv(args.device), bundle_id(args)
    name = "log.prev.txt" if args.prev else "log.txt"
    with tempfile.TemporaryDirectory() as tmp:
        local = os.path.join(tmp, name)
        run = devicectl("device", "copy", "from", "--device", tv["id"], "--domain-type", "appDataContainer",
                        "--domain-identifier", bundle, "--source", "%s/%s" % (DATA_DIR, name),
                        "--destination", local, check=False)
        if run.returncode != 0 or not os.path.exists(local):
            raise Failed("No %s on %s (%s): has the game run?\n%s" %
                         (name, tv["name"], bundle, (run.stderr or run.stdout).strip()))
        with open(local, errors="replace") as f:
            sys.stdout.write(f.read())


def main():
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("--device", help="the Apple TV's name or identifier (when more than one is paired)")
    common.add_argument("--app", help="the built gt3r.app (default: the newest in build-tvos/)")
    common.add_argument("--bundle-id", help="the app's bundle identifier (default: the built app's)")
    parser = argparse.ArgumentParser(description="GT3 Revisited on an Apple TV: install, start, read the log.")
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("devices", parents=[common], help="the paired Apple TVs").set_defaults(run=cmd_devices)
    build = sub.add_parser("build", parents=[common], help="configure and build the signed app")
    build.add_argument("--disc", help="a disc image (.iso, .chd) of your own to build into the app")
    build.add_argument("--no-disc", action="store_true", help="build without a disc image inside")
    build.add_argument("--team", help="your Apple development team ID (remembered by the build tree)")
    build.add_argument("--config", default="RelWithDebInfo", help="the build configuration")
    build.set_defaults(run=cmd_build)
    sub.add_parser("install", parents=[common], help="install the built app").set_defaults(run=cmd_install)
    launch = sub.add_parser("launch", parents=[common], help="start the game")
    launch.add_argument("-e", "--env", action="append", default=[], metavar="NAME=value",
                        help="an environment variable for the game (GT3R_...); may repeat")
    launch.add_argument("game_args", nargs=argparse.REMAINDER, help="after --: the game's own arguments")
    launch.set_defaults(run=cmd_launch)
    push = sub.add_parser("push-disc", parents=[common], help="send a disc image to the game's folder")
    push.add_argument("image", help="the .iso or .chd")
    push.set_defaults(run=cmd_push_disc)
    log = sub.add_parser("log", parents=[common], help="print the game's log")
    log.add_argument("--prev", action="store_true", help="the run before this one")
    log.set_defaults(run=cmd_log)
    reports = sub.add_parser("reports", parents=[common], help="why the last run ended")
    reports.add_argument("--count", type=int, default=5, help="how many of the newest reports to show")
    reports.set_defaults(run=cmd_reports)
    saves = sub.add_parser("saves", parents=[common], help="copy the settings, saves and replays to or from the Mac")
    saves.add_argument("action", choices=["pull", "push"])
    saves.add_argument("folder", nargs="?", help="pull: where the copy goes (default: a dated folder in "
                                                 "~/Documents/GT3 Revisited TV backup); push: the copy to put back")
    saves.set_defaults(run=cmd_saves)
    args = parser.parse_args()
    try:
        args.run(args)
    except Failed as e:
        sys.exit("tv.py: %s" % e)


if __name__ == "__main__":
    main()
