#!/usr/bin/env python3
"""
Generate a DHGR (Double Hi-Res) cassowary image for the casso-rocks demo.

Reads:  Assets/3a Mrs Cassowary closeup 8167.jpg
Writes: Apple2/Demos/dhgr-cassowary-aux.bin   (8 KB)
        Apple2/Demos/dhgr-cassowary-main.bin  (8 KB)

Pipeline:
  1. Crop the source jpg to the //e DHGR aspect ratio (140x192 effective
     color pixels — but rendered as 560x192 monochrome dots, with 4 dots
     forming one color via NTSC encoding).
  2. Quantize to the 16-color //e LoRes/DHGR palette (matches what
     AppleLoResMode.cpp / AppleDoubleHiResMode.cpp use to render).
  3. Encode each color pixel as a 4-bit nibble at the corresponding
     position in the aux+main interleaved byte stream:
       aux[0]=dots  0-6 (7 bits, bit0=leftmost)
       main[0]=dots 7-13
       aux[1]=dots  14-20
       main[1]=dots 21-27
       ...
     Each 4-dot group encodes one color. With 140 color pixels per row
     mapped to 4 dots each = 560 dots, evenly tiled.

The same HGR row offset formula applies (see HgrPreprocess.py).
"""

import sys
from pathlib import Path

try:
    from PIL import Image, ImageOps
except ImportError:
    sys.stderr.write("PIL/Pillow required: pip install Pillow\n")
    sys.exit(1)


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
DHGR_DOT_W   = 560
ROW_BYTES    = 40


def hgr_row_offset(row: int) -> int:
    return 1024 * (row & 7) + 128 * ((row >> 3) & 7) + 40 * (row >> 6)


def build_palette_image() -> Image.Image:
    """Create a 1x16 reference image using the //e palette so PIL can
    quantize against it."""
    pal = []
    for r, g, b in DHGR_PALETTE_RGB:
        pal.extend([r, g, b])
    # PIL needs a 768-byte palette (256 entries x 3 bytes); pad with zeros.
    pal.extend([0] * (768 - len(pal)))
    p_img = Image.new("P", (1, 1))
    p_img.putpalette(pal)
    return p_img


def quantize_to_dhgr_palette(src: Image.Image) -> Image.Image:
    """Crop to the DHGR display aspect, then quantize to the 16 //e
    colors. Real-//e DHGR pixels are roughly 1:2 (tall/narrow), so on
    screen the 140-color-wide x 192-row buffer renders at 4:3 — i.e.
    the rendered display is 560x192 dots squashed into a ~4:3 frame
    by the NTSC pipe. Casso's renderer doubles vertically to 560x384
    which is also ~4:3.

    Therefore the source must be cropped to a 4:3 aspect (width:height
    = 1.46:1, matching the rendered 560:384) before it's resized to
    140x192 — otherwise a portrait source like the cassowary photo
    gets squashed horizontally as PIL stretches it to fill the
    target. Use ImageOps.fit which crops centered to the target
    aspect, then resamples."""
    target_aspect = 560.0 / 384.0
    fitted = ImageOps.fit(
        src.convert("RGB"),
        (DHGR_COLOR_W * 4, DHGR_ROWS * 2),  # 4:3 work surface in real pixels
        method=Image.LANCZOS,
        bleed=0.0,
        centering=(0.5, 0.5))

    # Now resample to 140x192 (the DHGR color resolution).
    resized = fitted.resize((DHGR_COLOR_W, DHGR_ROWS), Image.LANCZOS)

    pal_img = build_palette_image()
    quantized = resized.quantize(palette=pal_img, dither=Image.FLOYDSTEINBERG)
    return quantized


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

    src = Image.open(src_path)
    print(f"loaded {src_path.name}: {src.size} {src.mode}")

    # Crop the source to a 4:3 aspect that matches DHGR's effective
    # color resolution (140:192 = ~7:9.6, but the rendered 560:192 with
    # square pixels is closer to 2.9:1). The 140x192 quantize step
    # lets us resize directly without worrying too much about pixel
    # aspect.
    quantized = quantize_to_dhgr_palette(src)
    print(f"quantized to {quantized.size} P-mode")

    aux_bytes, main_bytes = encode_dhgr(quantized)

    out_dir = repo / "Apple2" / "Demos"
    aux_path  = out_dir / "dhgr-cassowary-aux.bin"
    main_path = out_dir / "dhgr-cassowary-main.bin"
    aux_path.write_bytes(aux_bytes)
    main_path.write_bytes(main_bytes)
    print(f"wrote {aux_path.name} ({len(aux_bytes)} bytes)")
    print(f"wrote {main_path.name} ({len(main_bytes)} bytes)")

    # Also save a preview PNG so a human can sanity-check the
    # quantization without booting the demo.
    preview = quantized.convert("RGB").resize(
        (DHGR_COLOR_W * 4, DHGR_ROWS * 2), Image.NEAREST)
    preview_path = repo / "Apple2" / "Demos" / "dhgr-cassowary-preview.png"
    preview.save(preview_path)
    print(f"wrote {preview_path.name} (preview)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
