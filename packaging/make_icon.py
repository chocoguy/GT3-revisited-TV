#!/usr/bin/env python3
"""Draws the GT3 Revisited application icon (original artwork, no game assets).

Writes packaging/icon.png (1024x1024), packaging/icon.ico (Windows), the
Android launcher icons and the Apple TV asset catalog (packaging/tvos). Needs
Pillow:  python3 packaging/make_icon.py
"""
import json
import math
import os

from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
S = 1024


def lerp(a, b, t):
    return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(len(a)))


def road_points(offset, n=200):
    # A sweeping right-hander from the bottom left to the top right.
    pts = []
    for i in range(n + 1):
        t = i / n
        x = 120 + 820 * t + 140 * math.sin(t * math.pi) * 0.35
        y = 930 - 820 * (t ** 1.35) - 180 * math.sin(t * math.pi)
        # Normal offset (perpendicular to the curve direction).
        t2 = min(1.0, t + 1e-3)
        x2 = 120 + 820 * t2 + 140 * math.sin(t2 * math.pi) * 0.35
        y2 = 930 - 820 * (t2 ** 1.35) - 180 * math.sin(t2 * math.pi)
        dx, dy = x2 - x, y2 - y
        ln = math.hypot(dx, dy) or 1
        pts.append((x - dy / ln * offset, y + dx / ln * offset))
    return pts


def background(w, h):
    # The icon's dark gradient, top to bottom, opaque.
    column = Image.new("RGBA", (1, h))
    px = column.load()
    top, bottom = (36, 40, 58), (12, 13, 19)
    for y in range(h):
        px[0, y] = lerp(top, bottom, y / (h - 1)) + (255,)
    return column.resize((w, h), Image.NEAREST)


def road_art():
    # The road alone on a transparent square (S x S).
    road = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(road)
    d.polygon(road_points(-125) + road_points(125)[::-1], fill=(58, 60, 68, 255))
    d.line(road_points(-112), fill=(235, 235, 240, 255), width=16, joint="curve")
    d.line(road_points(112), fill=(235, 235, 240, 255), width=16, joint="curve")
    # Red/white kerb on the inside of the corner.
    kerb = road_points(150, 60)
    for i in range(len(kerb) - 1):
        d.line([kerb[i], kerb[i + 1]], fill=(220, 50, 40, 255) if i % 2 else (240, 240, 240, 255), width=26)
    # Yellow dashed centre line.
    centre = road_points(0, 120)
    for i in range(0, len(centre) - 1, 6):
        d.line([centre[i], centre[min(i + 3, len(centre) - 1)]], fill=(255, 196, 40, 255), width=18)
    return road


def draw(size=S):
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle((32, 32, S - 32, S - 32), radius=200, fill=255)
    img.paste(background(S, S), (0, 0), mask)

    road = road_art()
    road_mask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(road_mask).rounded_rectangle((32, 32, S - 32, S - 32), radius=200, fill=255)
    img.paste(road, (0, 0), Image.composite(road.getchannel("A"), Image.new("L", (S, S), 0), road_mask))

    # Rim highlight.
    ImageDraw.Draw(img).rounded_rectangle((34, 34, S - 34, S - 34), radius=198, outline=(255, 196, 40, 255), width=10)
    return img if size == S else img.resize((size, size), Image.LANCZOS)


# Apple TV (docs/dev/PLATFORMS.md "tvOS"): the Home screen takes only a layered icon, an image
# stack whose back layer is opaque (the layers shift against each other as the icon is focused),
# and wants the Top Shelf pictures in the same brand asset. tvOS rounds the corners itself.
XCODE_INFO = {"author": "xcode", "version": 1}


def tv_picture(w, h, back, front, art=5 / 3):
    # `back`: the gradient; `front`: the road, centred, its square `art` times the picture's
    # height: taller than the picture, so the road runs in at the bottom and out at the top.
    img = background(w, h) if back else Image.new("RGBA", (w, h), (0, 0, 0, 0))
    if front:
        side = round(h * art)
        road = road_art().resize((side, side), Image.LANCZOS)
        cw, ch = min(side, w), min(side, h)
        left, top = (side - cw) // 2, (side - ch) // 2
        img.alpha_composite(road.crop((left, top, left + cw, top + ch)), ((w - cw) // 2, (h - ch) // 2))
    return img


def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")


def tv_imageset(folder, w, h, scales, back, front):
    # An image set of one picture at each scale ("1x", "2x").
    images = []
    for scale in scales:
        k = int(scale[0])
        name = "image.png" if k == 1 else "image@%dx.png" % k
        os.makedirs(folder, exist_ok=True)
        tv_picture(w * k, h * k, back, front).save(os.path.join(folder, name), optimize=True)
        images.append({"filename": name, "idiom": "tv", "scale": scale})
    write_json(os.path.join(folder, "Contents.json"), {"images": images, "info": XCODE_INFO})


def tv_imagestack(folder, w, h, scales):
    layers = []
    for layer, back, front in (("Front", False, True), ("Back", True, False)):
        name = layer + ".imagestacklayer"
        write_json(os.path.join(folder, name, "Contents.json"), {"info": XCODE_INFO})
        tv_imageset(os.path.join(folder, name, "Content.imageset"), w, h, scales, back, front)
        layers.append({"filename": name})
    write_json(os.path.join(folder, "Contents.json"), {"info": XCODE_INFO, "layers": layers})


def tv_assets():
    catalog = os.path.join(HERE, "tvos", "gt3r-tvos.xcassets")
    brand = os.path.join(catalog, "AppIcon.brandassets")
    write_json(os.path.join(catalog, "Contents.json"), {"info": XCODE_INFO})
    tv_imagestack(os.path.join(brand, "App Icon.imagestack"), 400, 240, ["1x", "2x"])
    tv_imagestack(os.path.join(brand, "App Icon - App Store.imagestack"), 1280, 768, ["1x"])
    tv_imageset(os.path.join(brand, "Top Shelf Image.imageset"), 1920, 720, ["1x", "2x"], True, True)
    tv_imageset(os.path.join(brand, "Top Shelf Image Wide.imageset"), 2320, 720, ["1x", "2x"], True, True)
    write_json(os.path.join(brand, "Contents.json"), {
        "assets": [
            {"filename": "App Icon.imagestack", "idiom": "tv", "role": "primary-app-icon", "size": "400x240"},
            {"filename": "App Icon - App Store.imagestack", "idiom": "tv", "role": "primary-app-icon",
             "size": "1280x768"},
            {"filename": "Top Shelf Image.imageset", "idiom": "tv", "role": "top-shelf-image", "size": "1920x720"},
            {"filename": "Top Shelf Image Wide.imageset", "idiom": "tv", "role": "top-shelf-image-wide",
             "size": "2320x720"},
        ],
        "info": XCODE_INFO,
    })


def main():
    icon = draw()
    icon.save(os.path.join(HERE, "icon.png"))
    icon.save(os.path.join(HERE, "icon.ico"), sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    res = os.path.join(ROOT, "android", "app", "src", "main", "res")
    for name, px in {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}.items():
        out = os.path.join(res, "mipmap-" + name)
        os.makedirs(out, exist_ok=True)
        icon.resize((px, px), Image.LANCZOS).save(os.path.join(out, "ic_launcher.png"))
    tv_assets()
    print("icons written")


if __name__ == "__main__":
    main()
