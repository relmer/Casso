#!/usr/bin/env python
# ============================================================================
#  HgrPreprocess.py — convert a source image into an Apple //e HGR framebuffer
#
#  Reads an arbitrary RGB image (typically one of the cassowary photos under
#  Assets/), resizes to 280x192, dithers to a black/white palette, and emits
#  the 8192-byte HGR page-1 framebuffer as raw bytes.
#
#  Determinism: PIL.Image.resize with a fixed resampling filter and
#  Image.convert('1', dither=Image.FLOYDSTEINBERG) are deterministic across
#  Pillow >= 9 on the same input bytes, so re-running the script reproduces
#  the same output.hash test fixture used by BootDiskTests.
#
#  HGR memory layout (Apple //e, page 1 at $2000):
#    addr = $2000
#         + 1024 * (row & 7)
#         + 128  * ((row >> 3) & 7)
#         +  40  * (row >> 6)
#  Each byte stores 7 horizontal pixels in bits 0..6 (LSB = leftmost).
#  Bit 7 selects the colour group (purple/green vs blue/orange); we leave
#  it 0 (purple/green) since this script only emits monochrome dithering.
# ============================================================================

import argparse
import sys

from pathlib import Path

from PIL import Image


HGR_WIDTH    = 280
HGR_HEIGHT   = 192
HGR_BYTES    = 8192
ROW_BYTES    = 40


def hgr_row_offset (row):
    return (1024 * (row & 7)
          +  128 * ((row >> 3) & 7)
          +   40 * (row >> 6))


def image_to_hgr (img):
    if img.size != (HGR_WIDTH, HGR_HEIGHT):
        raise ValueError ("image must be 280x192 before bit-packing")

    pixels = img.load ()
    out    = bytearray (HGR_BYTES)

    for row in range (HGR_HEIGHT):
        base = hgr_row_offset (row)

        for col_byte in range (ROW_BYTES):
            packed = 0

            for bit in range (7):
                x = col_byte * 7 + bit

                if pixels[x, row] != 0:
                    packed |= (1 << bit)

            out[base + col_byte] = packed

    return bytes (out)


def load_and_fit (src_path):
    img = Image.open (src_path).convert ("RGB")

    src_w, src_h = img.size
    target_aspect = HGR_WIDTH / HGR_HEIGHT
    src_aspect    = src_w / src_h

    if src_aspect > target_aspect:
        new_w = int (src_h * target_aspect)
        crop_x = (src_w - new_w) // 2
        img    = img.crop ((crop_x, 0, crop_x + new_w, src_h))
    else:
        new_h = int (src_w / target_aspect)
        crop_y = (src_h - new_h) // 2
        img    = img.crop ((0, crop_y, src_w, crop_y + new_h))

    img = img.resize ((HGR_WIDTH, HGR_HEIGHT), Image.LANCZOS)
    img = img.convert ("L")
    img = img.convert ("1", dither=Image.FLOYDSTEINBERG)

    return img


def main ():
    parser = argparse.ArgumentParser (description=__doc__)
    parser.add_argument ("--input",  required=True, help="source image path")
    parser.add_argument ("--output", required=True, help="raw HGR framebuffer output path")
    args = parser.parse_args ()

    src = Path (args.input)
    dst = Path (args.output)

    if not src.is_file ():
        print (f"error: source image not found: {src}", file=sys.stderr)
        return 1

    img  = load_and_fit (src)
    data = image_to_hgr (img)

    if len (data) != HGR_BYTES:
        print (f"error: expected {HGR_BYTES} bytes of HGR, got {len (data)}", file=sys.stderr)
        return 1

    dst.parent.mkdir (parents=True, exist_ok=True)
    dst.write_bytes (data)

    print (f"wrote {len (data)} bytes -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit (main ())
