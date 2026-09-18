#!/usr/bin/env python3
"""
GIMP-style PNG8 Floyd-Steinberg ditherer.

Windows:
    Drag PNG files or a folder onto this .py file.

Behavior:
    - RGB: 15-color adaptive palette + Floyd-Steinberg dithering.
    - Alpha below 6.25%: completely transparent.
    - Alpha from 6.25% to 100%: converted to a 1-bit
      Floyd-Steinberg transparency dither.
    - 100% alpha: completely opaque.
    - Output: indexed PNG8, with 15 RGB colors + 1 transparent
      palette entry when transparency is present.

Requires Pillow:
    py -m pip install pillow
"""

import os
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    print("Pillow is not installed.")
    print("Install it with:")
    print("    py -m pip install pillow")
    input("\nPress Enter to exit...")
    raise SystemExit(1)


# -----------------------------
# Settings
# -----------------------------
RGB_COLORS = 15
ALPHA_CUTOFF = 0.0625
DITHER_MATRIX = [
    (7, 16),
    (3, 16),
    (5, 16),
    (1, 16),
]
# Floyd-Steinberg:
#       *  7/16
# 3/16  5/16  1/16


def floyd_steinberg_binary(values, width, height):
    """
    Floyd-Steinberg dither a grayscale 0..255 image into
    0 or 255.

    values is a flat list of floats.
    """
    work = values[:]
    out = [0] * (width * height)

    for y in range(height):
        for x in range(width):
            i = y * width + x

            old = work[i]
            new = 255.0 if old >= 128.0 else 0.0
            out[i] = 255 if new else 0

            error = old - new

            if x + 1 < width:
                work[i + 1] += error * 7.0 / 16.0

            if y + 1 < height:
                if x > 0:
                    work[i + width - 1] += error * 3.0 / 16.0

                work[i + width] += error * 5.0 / 16.0

                if x + 1 < width:
                    work[i + width + 1] += error * 1.0 / 16.0

    return out


def make_alpha_dither(img):
    """
    Convert original alpha into binary transparency.

    0..24.999% -> 0 (transparent)

    6.25..100% -> remap to 0..255, then Floyd-Steinberg
    dither that value into transparent/opaque pixels.

    Thus:
        6.25% -> essentially transparent
        50%  -> approximately 1/3 opaque coverage
        75%  -> approximately 2/3 opaque coverage
        100% -> solid
    """
    rgba = img.convert("RGBA")
    alpha = rgba.getchannel("A")

    w, h = rgba.size
    src = list(alpha.getdata())

    values = []

    for a in src:
        a01 = a / 255.0

        if a01 < ALPHA_CUTOFF:
            values.append(0.0)
        else:
            # 25% becomes 0%, 100% becomes 100%.
            remapped = (a01 - ALPHA_CUTOFF) / (1.0 - ALPHA_CUTOFF)
            remapped = max(0.0, min(1.0, remapped))
            values.append(remapped * 255.0)

    result = floyd_steinberg_binary(values, w, h)

    return Image.frombytes("L", (w, h), bytes(result))


def make_rgb_palette(img):
    """
    Make a 15-color adaptive RGB palette using Pillow's
    Floyd-Steinberg indexed conversion.

    Alpha is removed first so transparent pixels cannot
    contaminate the color palette.
    """
    rgb = img.convert("RGB")

    # Pillow's MEDIANCUT finds an adaptive palette similar
    # to GIMP's adaptive indexed conversion.
    indexed = rgb.quantize(
        colors=RGB_COLORS,
        method=Image.Quantize.MEDIANCUT,
        dither=Image.Dither.FLOYDSTEINBERG,
    )

    return indexed


def build_png8(rgb_indexed, alpha_mask):
    """
    Combine indexed RGB with the binary alpha dither.

    Palette:
        0..14 = RGB colors
        15    = transparent

    Opaque pixels keep their RGB palette index.
    Transparent pixels use the dedicated transparent entry.
    """
    w, h = rgb_indexed.size
    rgb_pixels = list(rgb_indexed.getdata())
    alpha_pixels = list(alpha_mask.getdata())

    has_transparency = any(a == 0 for a in alpha_pixels)

    # Copy the RGB palette.
    source_palette = rgb_indexed.getpalette() or []
    source_palette = source_palette[:RGB_COLORS * 3]

    while len(source_palette) < RGB_COLORS * 3:
        source_palette.extend([0, 0, 0])

    if not has_transparency:
        # No transparency: use the normal 15-color indexed image.
        out = Image.new("P", (w, h))
        out.putdata(rgb_pixels)
        out.putpalette(source_palette + [0] * (256 * 3 - len(source_palette)))
        return out

    transparent_index = RGB_COLORS

    out_pixels = []
    for p, a in zip(rgb_pixels, alpha_pixels):
        if a == 0:
            out_pixels.append(transparent_index)
        else:
            out_pixels.append(min(p, RGB_COLORS - 1))

    out = Image.new("P", (w, h))
    out.putdata(out_pixels)

    palette = source_palette + [0, 0, 0]
    palette += [0] * (256 * 3 - len(palette))
    out.putpalette(palette)

    # PNG tRNS: palette entry 15 is fully transparent.
    transparency = [255] * (transparent_index + 1)
    transparency[transparent_index] = 0
    out.info["transparency"] = bytes(transparency)

    return out


def process_png(path):
    print(f"Processing: {path}")

    with Image.open(path) as source:
        img = source.convert("RGBA")

    alpha = make_alpha_dither(img)
    rgb = make_rgb_palette(img)
    output = build_png8(rgb, alpha)

    # Keep the original name and replace it.
    # A temporary file prevents corruption if saving fails.
    temp = path.with_name(path.stem + "_png8_temp.png")

    output.save(
        temp,
        format="PNG",
        optimize=False,
        bits=8,
    )

    os.replace(temp, path)

    print(f"  Done: {path}")


def collect_pngs(items):
    files = []

    for item in items:
        p = Path(item)

        if p.is_file() and p.suffix.lower() == ".png":
            files.append(p)

        elif p.is_dir():
            for f in p.rglob("*.png"):
                if f.is_file():
                    files.append(f)

    return sorted(set(files))


def main():
    if len(sys.argv) < 2:
        print("Drag PNG files or a folder onto this script.")
        print()
        print("Or run:")
        print("    py gimp_png8_dither.py image.png")
        print("    py gimp_png8_dither.py folder")
        input("\nPress Enter to exit...")
        return

    files = collect_pngs(sys.argv[1:])

    if not files:
        print("No PNG files found.")
        input("\nPress Enter to exit...")
        return

    print("==========================================")
    print(" GIMP-STYLE PNG8 FLOYD-STEINBERG DITHER")
    print("==========================================")
    print(f"Files: {len(files)}")
    print("RGB colors: 15")
    print("Alpha cutoff: 6.25%")
    print("Alpha dithering: Floyd-Steinberg")
    print()

    success = 0

    for path in files:
        # Avoid processing our temporary output.
        if path.name.endswith("_png8_temp.png"):
            continue

        try:
            process_png(path)
            success += 1
        except Exception as e:
            print(f"  ERROR: {e}")

    print()
    print("==========================================")
    print(f"Finished: {success}/{len(files)}")
    print("==========================================")
    input("\nPress Enter to exit...")


if __name__ == "__main__":
    main()
