#!/usr/bin/env python3
"""
Generate the casso-rocks demo's two HGR cassowary images.

Reads:  Assets/3a Mrs Cassowary closeup 8167.jpg
Writes: cassowary.hgr       (8 KB, 6-color NTSC artifact)
        cassowary-mono.hgr  (8 KB, authored for 280x192 1-bit)
        plus a preview PNG of each.

The DHGR pair's argument, one resolution down: an HGR framebuffer is
also read two ways, and the two readings want opposite things from the
encoder, so the demo ships both and asks which one to show. See
DhgrCassowaryGen for the long form.

WHAT IS DIFFERENT ABOUT HGR MONOCHROME. It is not a 560-dot canvas.
Per AppleHiResMode's monochrome pass, a lit pixel x paints TWO adjacent
half-dots, at 2x+s and 2x+s+1, where s is bit 7 of the byte that pixel
lives in. So:

  - every mark is two half-dots wide; a single isolated half-dot cannot
    be drawn at all,
  - the half-dot phase s is shared by all seven pixels in a byte,
  - a byte with s=1 spills one half-dot into the next byte's territory,
    and cannot reach its own first slot.

That leaves 256 achievable 14-half-dot patterns per byte out of 2^14.
The useful way to read it is that HGR monochrome is a 280x192 one-bit
image plus a per-byte half-PIXEL nudge, so this encoder dithers at 280
and then picks bit 7 per byte against the finer 560 target. Two more
elaborate schemes were tried and are worse, which is worth recording so
nobody spends the afternoon again: diffusing error against a 560 target
in 14-slot blocks streaks badly, because the horizontal error has
nowhere to go once fourteen slots are committed at once; and a
least-squares refinement scoring candidates through a small blur
collapses into vertical stripe texture, because a blurred metric cannot
tell a stripe pattern from the right local density.

WHY THE HGR PAIR IS CROPPED TIGHTER than the DHGR pair. The portrait
crop letterboxes to about 121 of the 280 columns, and at half the
horizontal resolution that is not enough bird. Cropping below the
wattles gets it to about 164, and the background gets a depth-of-field
blur so its dither goes smooth instead of noisy and the dots that are
left are spent on the head.
"""

import sys
from pathlib import Path

try:
    from PIL import Image, ImageFilter
except ImportError:
    sys.stderr.write("PIL/Pillow required: pip install Pillow\n")
    sys.exit(1)

sys.path.insert(0, str(Path(__file__).resolve().parent))
import DemoImageLayout as Layout
import HgrPreprocess


BYTES_PER_ROW = 40
PIX_PER_BYTE  = 7

# Casque, head, and the top of the wattles. Tighter than the DHGR pair's
# CROP_PORTRAIT, for the resolution reason in the module docstring.
CROP_HGR = (60, 40, 860, 820)

MONO_GAMMA    = 1.45
MONO_CONTRAST = 1.45
MONO_SHARPEN  = 2.00
MONO_DOF      = 2.50    # background blur radius, in canvas units
MONO_VIGNETTE = 0.35


def build_candidates():
    """Every achievable (shift, pixel-bits) pattern for one byte.

    Returns masks[256][15] over half-dot slots 14b+0 .. 14b+14 -- the
    last being the spill into the next byte -- and the byte value that
    produces each."""
    masks  = [[0] * 15 for _ in range(256)]
    values = [0] * 256

    for shift in (0, 1):
        for bits in range(128):
            idx = shift * 128 + bits
            values[idx] = bits | (0x80 if shift else 0x00)
            for bit in range(PIX_PER_BYTE):
                if not (bits >> bit) & 1:
                    continue
                slot = 2 * bit + shift
                masks[idx][slot]     = 1
                masks[idx][slot + 1] = 1

    return masks, values


MASKS, VALUES = build_candidates()


def photo_canvas(width, mono):
    """The photo, fitted into the page's photo box at `width` units."""
    canvas = Image.new("L" if mono else "RGB", (width, Layout.ROWS),
                       0 if mono else (0, 0, 0))
    photo, at = Layout.fit_photo(
        Layout.load_photo("L" if mono else "RGB", CROP_HGR), width)
    canvas.paste(photo, at)
    return canvas, photo.size, at


def mono_target(width):
    """The grayscale the monochrome encoder aims at, at `width` units."""
    canvas, (pw, ph), (x0, y0) = photo_canvas(width, mono=True)

    # Depth of field: blur the background so its dither comes out smooth
    # rather than noisy, which leaves the limited dot budget to the head.
    # Centered on the bird rather than on the frame.
    soft   = canvas.filter(ImageFilter.GaussianBlur(
        MONO_DOF * width / float(Layout.DOTS)))
    sharp  = canvas.load()
    blur   = soft.load()
    cx     = x0 + pw * 0.55
    cy     = y0 + ph * 0.58

    for y in range(Layout.ROWS):
        for x in range(width):
            dx = (x - cx) / (pw * 0.42)
            dy = (y - cy) / (ph * 0.42)
            r  = (dx * dx + dy * dy) ** 0.5
            m  = min(1.0, max(0.0, (r - 0.75) / 0.6))
            if m > 0.0:
                sharp[x, y] = int(sharp[x, y] * (1.0 - m) + blur[x, y] * m)

    canvas = Layout.apply_tone(canvas, MONO_GAMMA, MONO_CONTRAST, MONO_SHARPEN)

    # Vignette, to push the frame edges down and the bird forward.
    pixels = canvas.load()
    ecx    = x0 + pw / 2.0
    ecy    = y0 + ph / 2.0
    for y in range(Layout.ROWS):
        for x in range(width):
            dx = (x - ecx) / (pw / 2.0)
            dy = (y - ecy) / (ph / 2.0)
            r  = (dx * dx + dy * dy) ** 0.5
            fade = 1.0 - MONO_VIGNETTE * max(0.0, r - 0.55) / 0.45
            if fade < 1.0:
                pixels[x, y] = int(pixels[x, y] * max(0.0, fade))

    return canvas


def choose_shifts(bits_row, target560, row):
    """Pixel bits are already decided; pick bit 7 per byte for the
    better fit against the finer 560-half-dot target."""
    out = bytearray(BYTES_PER_ROW)

    for b in range(BYTES_PER_ROW):
        packed = 0
        for bit in range(PIX_PER_BYTE):
            if bits_row[b * PIX_PER_BYTE + bit]:
                packed |= 1 << bit

        best      = VALUES[packed]
        best_cost = None
        for shift in (0, 1):
            idx  = shift * 128 + packed
            cost = 0
            for i in range(14):
                slot = 14 * b + i
                want = target560[slot] if slot < Layout.DOTS else 0
                diff = MASKS[idx][i] * 255 - want
                cost += diff * diff
            if best_cost is None or cost < best_cost:
                best_cost, best = cost, VALUES[idx]

        out[b] = best

    return out


def build_mono():
    """The monochrome HGR framebuffer."""
    band   = Layout.band_rows()
    chrome = Layout.chrome_cells()

    t280 = mono_target(Layout.HGR_PIX)
    t560 = mono_target(Layout.DOTS)
    bits = Layout.dither_1bit(t280, skip_rows=band).load()
    fine = t560.load()

    band_set = set(band)
    out      = bytearray(8192)

    for row in range(Layout.ROWS):
        if row in band_set:
            # The title band is drawn, not dithered. One 140-grid cell is
            # two HGR pixels, and two adjacent lit pixels read as WHITE on
            # a color monitor and four lit half-dots on a monochrome one --
            # the same dual-decode property the DHGR title relies on.
            rowbytes = bytearray(BYTES_PER_ROW)
            for cell, r in chrome:
                if r != row:
                    continue
                for px in (cell * 2, cell * 2 + 1):
                    rowbytes[px // PIX_PER_BYTE] |= 1 << (px % PIX_PER_BYTE)
        else:
            bits_row   = [bits[x, row] != 0 for x in range(Layout.HGR_PIX)]
            target_row = [fine[x, row] for x in range(Layout.DOTS)]
            rowbytes   = choose_shifts(bits_row, target_row, row)

        base = Layout.hgr_row_offset(row)
        out[base : base + BYTES_PER_ROW] = rowbytes

    return bytes(out)


def build_color():
    """The 6-color artifact HGR framebuffer, through HgrPreprocess's
    per-byte palette classifier."""
    canvas, _, _ = photo_canvas(Layout.HGR_PIX, mono=False)
    pixels       = canvas.load()

    band     = set(Layout.band_rows())
    chrome   = Layout.chrome_cells()
    out      = bytearray(8192)

    for row in range(Layout.ROWS):
        base = Layout.hgr_row_offset(row)

        if row in band:
            rowbytes = bytearray(BYTES_PER_ROW)
            for cell, r in chrome:
                if r != row:
                    continue
                for px in (cell * 2, cell * 2 + 1):
                    rowbytes[px // PIX_PER_BYTE] |= 1 << (px % PIX_PER_BYTE)
            out[base : base + BYTES_PER_ROW] = rowbytes
            continue

        for col in range(BYTES_PER_ROW):
            out[base + col] = HgrPreprocess.encode_byte(pixels, row, col)

    return bytes(out)


def render_mono(buf):
    """Mirror of AppleHiResMode's monochrome pass, for the preview."""
    img    = Image.new("L", (Layout.DOTS, Layout.ROWS), 0)
    pixels = img.load()

    for row in range(Layout.ROWS):
        base = Layout.hgr_row_offset(row)
        for b in range(BYTES_PER_ROW):
            data  = buf[base + b]
            shift = 1 if (data & 0x80) else 0
            for bit in range(PIX_PER_BYTE):
                if not (data & (1 << bit)):
                    continue
                slot = 2 * (b * PIX_PER_BYTE + bit) + shift
                if slot < Layout.DOTS:
                    pixels[slot, row] = 255
                if slot + 1 < Layout.DOTS:
                    pixels[slot + 1, row] = 255

    return img


def main():
    #  The assets sit beside this script now, so the output folder is
    #  simply this one.
    out_dir = Path(__file__).resolve().parent

    color = build_color()
    mono  = build_mono()

    (out_dir / "cassowary.hgr").write_bytes(color)
    (out_dir / "cassowary-mono.hgr").write_bytes(mono)
    print(f"wrote cassowary.hgr ({len(color)} bytes)")
    print(f"wrote cassowary-mono.hgr ({len(mono)} bytes)")

    render_mono(mono).convert("RGB") \
        .resize((Layout.DOTS, Layout.ROWS * 2), Image.NEAREST) \
        .save(out_dir / "cassowary-mono-preview.png")
    print("wrote cassowary-mono-preview.png")

    return 0


if __name__ == "__main__":
    sys.exit(main())
