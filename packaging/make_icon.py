#!/usr/bin/env python3
"""Makes the other platforms' icons from the GT3 Revisited artwork (no game assets).

The artwork itself is not drawn here: packaging/icon.png (1024x1024) and the Apple TV asset catalog
packaging/tvos/gt3r-tvos.xcassets (layered icon, Top Shelf pictures, launch screen logo) are the
project's art, replaced as files. This script only scales packaging/icon.png into
packaging/icon.ico (Windows) and the Android launcher icons, and never writes the art.
The macOS .icns and the installer's AppIcon.icns are made with sips and iconutil
(cmake/Packaging.cmake). Needs Pillow:  python3 packaging/make_icon.py
"""
import os

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def main():
    icon = Image.open(os.path.join(HERE, "icon.png")).convert("RGBA")
    if icon.size != (1024, 1024):
        raise SystemExit("packaging/icon.png must be 1024x1024 (it is %dx%d)" % icon.size)
    icon.save(os.path.join(HERE, "icon.ico"), sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    res = os.path.join(ROOT, "android", "app", "src", "main", "res")
    for name, px in {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}.items():
        out = os.path.join(res, "mipmap-" + name)
        os.makedirs(out, exist_ok=True)
        icon.resize((px, px), Image.LANCZOS).save(os.path.join(out, "ic_launcher.png"))
    print("icons written from packaging/icon.png")


if __name__ == "__main__":
    main()
