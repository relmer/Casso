#!/usr/bin/env python3
"""
Generate a DHGR (Double Hi-Res) cassowary image for the casso-rocks demo.

Reads:  Assets/3a Mrs Cassowary closeup 8167.jpg
Writes: Apple2/Demos/dhgr-cassowary-aux.bin   (8 KB)
        Apple2/Demos/dhgr-cassowary-main.bin  (8 KB)

Visual goal: produce a DHGR rendering that matches the HGR
cassowary one-for-one in framing and the centered "Casso" title —
just smoother colour gradients thanks to 16-color Floyd-Steinberg
dithering instead of HGR's per-byte palette classification.

Pipeline:
  1. Reuse HgrPreprocess.crop_and_fit to letterbox the source into
     a 280x192 HGR canvas with the title painted on top. This is
     identical to what the HGR generator does, so the framing,
     subject crop, and title position are guaranteed to match.
  2. Resize that 280x192 canvas to 140x192 (DHGR's color-pixel
     resolution — 1 color per 4 dots). Each source column
     compresses 2:1 horizontally, but on display the DHGR
     renderer expands every color cell back to 4 dots, so the
     final on-screen aspect is identical to HGR's.
  3. Quantize to the 16 //e LoRes/DHGR colors with
     Floyd-Steinberg error diffusion.
  4. Encode each color cell as a 4-bit nibble at the
     corresponding position in the aux+main interleaved byte
     stream, with the same row-offset formula as HGR.
"""

import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:
    sys.stderr.write("PIL/Pillow required: pip install Pillow\n")
    sys.exit(1)

# Reach the sibling HGR pipeline module.
sys.path.insert(0, str(Path(__file__).resolve().parent))
import HgrPreprocess


# Apple //e 16-color LoRes/DHGR palette (RGB), index = 4-bit color value.
# Must match CassoEmuCore/Video/AppleLoResMode.cpp::kLoResColors.
DHGR_PALETTE_RGB = [
    (  0,   0,   0),   #  0 Black
    (221,  34, 102),   #  1 Magenta
    (  0,   0, 153),   #  2 Dark Blue
    (221,   0,  68),   #  3 Purple
    (  0,  34,   0),   #  4 Dark Green
    ( 85,  85,  85),   #  5 Grey 1
    (  0,  34, 204),   #  6 Medium Blue
    (102, 170, 255),   #  7 Light Blue
    (136,  85,   0),   #  8 Brown
    (255,  68,   0),   #  9 Orange
    (170, 170, 170),   # 10 Grey 2
    (255, 136, 136),   # 11 Pink
    (  0, 221,   0),   # 12 Light Green
    (255, 255,   0),   # 13 Yellow
    ( 68, 255, 221),   # 14 Aquamarine
    (255, 255, 255),   # 15 White
]

DHGR_COLOR_W = 140
DHGR_ROWS    = 192
ROW_BYTES    = 40


def hgr_row_offset(row: int) -> int:
    return 1024 * (row & 7) + 128 * ((row >> 3) & 7) + 40 * (row >> 6)


def build_palette_image() -> Image.Image:
    """Create a reference image using the //e palette so PIL can
    quantize against it."""
    pal = []
    for r, g, b in DHGR_PALETTE_RGB:
        pal.extend([r, g, b])
    pal.extend([0] * (768 - len(pal)))
    p_img = Image.new("P", (1, 1))
    p_img.putpalette(pal)
    return p_img


def encode_dhgr(quantized: Image.Image) -> tuple[bytes, bytes]:
    """Encode the 140x192 quantized image (P mode, palette indices 0-15)
    into 8 KB aux + 8 KB main DHGR byte streams."""
    aux_buf  = bytearray(8192)
    main_buf = bytearray(8192)

    pixels = quantized.load()

    for row in range(DHGR_ROWS):
        base = hgr_row_offset(row)
        # 80 bytes per scanline: aux[0] main[0] aux[1] main[1] ... aux[39] main[39]
        # 7 dots per byte, bit 0 = leftmost dot, bit 7 ignored.
        # 140 color pixels per row, each spanning 4 dots.
        for color_x in range(DHGR_COLOR_W):
            color = pixels[color_x, row] & 0x0F
            for nibble_bit in range(4):
                dot = color_x * 4 + nibble_bit
                if not ((color >> nibble_bit) & 1):
                    continue
                byte_idx = dot // 7
                bit_idx  = dot - byte_idx * 7
                if (byte_idx & 1) == 0:
                    aux_buf[base + (byte_idx >> 1)] |= (1 << bit_idx)
                else:
                    main_buf[base + (byte_idx >> 1)] |= (1 << bit_idx)

    return bytes(aux_buf), bytes(main_buf)


def main():
    repo = Path(__file__).resolve().parent.parent
    src_path = repo / "Assets" / "3a Mrs Cassowary closeup 8167.jpg"
    if not src_path.exists():
        sys.stderr.write(f"source image not found: {src_path}\n")
        return 1

    # Step 1: letterbox the source into the HGR canvas (280x192) using
    # the exact same crop, aspect handling, and title painting that
    # HgrPreprocess uses for the HGR cassowary. Guarantees the DHGR
    # version frames the bird identically and shows "Casso" at the
    # same screen position.
    canvas = HgrPreprocess.crop_and_fit(
        src_path,
        crop_box=HgrPreprocess.DEFAULT_CROP,
        letterbox=True,
        title="Casso",
        title_size=18,
        title_stroke=0)
    print(f"letterboxed canvas: {canvas.size}")

    # Step 2: resize the HGR-resolution canvas down to DHGR color
    # resolution (140x192). DHGR will expand each color cell back to
    # 4 dots on display, so on-screen aspect matches HGR.
    resized = canvas.resize((DHGR_COLOR_W, DHGR_ROWS), Image.LANCZOS)

    # Step 3: quantize to the 16 //e colors with Floyd-Steinberg.
    pal_img = build_palette_image()
    quantized = resized.quantize(palette=pal_img, dither=Image.FLOYDSTEINBERG)
    print(f"quantized: {quantized.size} P-mode")

    # Step 4: encode to DHGR aux+main byte streams.
    aux_bytes, main_bytes = encode_dhgr(quantized)

    out_dir = repo / "Apple2" / "Demos"
    aux_path  = out_dir / "dhgr-cassowary-aux.bin"
    main_path = out_dir / "dhgr-cassowary-main.bin"
    aux_path.write_bytes(aux_bytes)
    main_path.write_bytes(main_bytes)
    print(f"wrote {aux_path.name} ({len(aux_bytes)} bytes)")
    print(f"wrote {main_path.name} ({len(main_bytes)} bytes)")

    # Also save a preview PNG (at the on-screen aspect: 560x384, with
    # each color cell expanded to 4x2 display pixels) so a human can
    # sanity-check without booting the demo.
    preview = quantized.convert("RGB").resize(
        (DHGR_COLOR_W * 4, DHGR_ROWS * 2), Image.NEAREST)
    preview_path = out_dir / "dhgr-cassowary-preview.png"
    preview.save(preview_path)
    print(f"wrote {preview_path.name} (preview)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
