#!/usr/bin/env python
# ============================================================================
#  HgrPreprocess.py — convert a source image into an Apple //e HGR framebuffer
#
#  Reads an arbitrary RGB image (typically one of the cassowary photos under
#  Assets/), crops to the desired subject region, fits it into 280x192 with
#  optional letterbox padding for portrait subjects, color-quantizes to
#  Apple ][ HGR's per-byte palette pairs (white/black plus violet+green or
#  blue+orange depending on each byte's high bit), and emits the 8192-byte
#  HGR page-1 framebuffer as raw bytes.
#
#  HGR memory layout (Apple //e, page 1 at $2000):
#    addr = $2000
#         + 1024 * (row & 7)
#         + 128  * ((row >> 3) & 7)
#         +  40  * (row >> 6)
#  Each byte stores 7 horizontal pixels in bits 0..6 (LSB = leftmost).
#  Bit 7 picks the colour palette pair: 0 = violet+green, 1 = blue+orange.
#  Within a byte, even-bit pixels (0,2,4,6) take the "phase A" colour
#  (violet or blue), odd-bit pixels (1,3,5) take the "phase B" colour
#  (green or orange). Two adjacent ON pixels appear as white due to NTSC
#  composite artefacting, two adjacent OFF pixels appear as black.
#
#  Determinism: PIL.Image.resize with a fixed resampling filter and our
#  own pure-python quantizer are deterministic across Pillow >= 9 on the
#  same input bytes, so re-running the script reproduces the same output.
# ============================================================================

import argparse
import sys

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


HGR_WIDTH      = 280
HGR_HEIGHT     = 192
HGR_BYTES      = 8192
ROW_BYTES      = 40
PIXELS_PER_BYTE = 7

# Default source-image crop, picked to capture the cassowary's full
# casque + head + neck + wattles from the standard portrait photo
# (`Assets/3a Mrs Cassowary closeup 8167.jpg`, 880x1600). Pass --crop
# on the command line to override for other source images.
DEFAULT_CROP   = (60, 40, 860, 1100)


# Approximate sRGB values for the six Apple ][ HGR colours. Sourced from
# the standard NTSC HGR palette commonly used by emulators (AppleWin,
# Virtual ][), close to what real composite hardware produces.
HGR_BLACK   = (0,   0,   0  )
HGR_WHITE   = (255, 255, 255)
HGR_VIOLET  = (170, 30,  220)
HGR_GREEN   = (40,  220, 60 )
HGR_BLUE    = (30,  130, 255)
HGR_ORANGE  = (255, 130, 30 )

# For each palette (bit7=0 / bit7=1), per-pixel-phase colours:
#   phase A = even bit position in the byte (bits 0,2,4,6)
#   phase B = odd  bit position in the byte (bits 1,3,5)
PALETTE_VG = (HGR_VIOLET, HGR_GREEN)    # bit7 = 0
PALETTE_BO = (HGR_BLUE,   HGR_ORANGE)   # bit7 = 1


def hgr_row_offset (row):
    return (1024 * (row & 7)
          +  128 * ((row >> 3) & 7)
          +   40 * (row >> 6))


def colour_distance_sq (a, b):
    dr = a[0] - b[0]
    dg = a[1] - b[1]
    db = a[2] - b[2]
    return dr * dr + dg * dg + db * db


def nearest_hgr_target (src):
    # Classify source pixel into one of the 6 distinct HGR-rendered
    # colours by sRGB distance, with one bias: WHITE only wins on
    # pixels that are both very bright AND very desaturated.
    # Otherwise prefer the closest chromatic colour. This keeps the
    # cassowary's pale-but-blue head from collapsing to solid white.
    r, g, b = src
    luma    = (r + g + b) // 3
    chroma  = max (r, g, b) - min (r, g, b)

    # Hard bright + grey = WHITE. Otherwise pick the nearest chromatic
    # colour (or BLACK).
    if luma >= 220 and chroma <= 30:
        return "WHITE"

    candidates = (
        ("BLACK",  HGR_BLACK ),
        ("VIOLET", HGR_VIOLET),
        ("GREEN",  HGR_GREEN ),
        ("BLUE",   HGR_BLUE  ),
        ("ORANGE", HGR_ORANGE),
    )
    best_name = "BLACK"
    best_err  = None
    for name, ref in candidates:
        err = colour_distance_sq (src, ref)
        if best_err is None or err < best_err:
            best_err  = err
            best_name = name

    # Very dim pixels: keep them BLACK rather than smearing a chromatic
    # speckle through dark feathers / shadow.
    if luma < 50 and best_name != "BLACK":
        if colour_distance_sq (src, HGR_BLACK) < best_err * 2:
            return "BLACK"

    return best_name


def encode_byte (pixels, row, col_byte):
    # Step 1: classify each of the 7 source pixels into an HGR target.
    targets = []
    for bit in range (PIXELS_PER_BYTE):
        x = col_byte * PIXELS_PER_BYTE + bit
        targets.append (nearest_hgr_target (pixels[x, row]))

    # Step 2: pick the palette pair (bit 7 of the byte). Weight blue/
    # orange votes a little higher than violet/green so that colour
    # pixels of those palettes pull the byte even when they're
    # outnumbered by neutral neighbours -- otherwise a single blue
    # pixel surrounded by leaf-green ones loses the palette vote and
    # gets demoted to black or wrong-colour artefact.
    bo_votes = sum (2 for t in targets if t in ("BLUE", "ORANGE"))
    vg_votes = sum (1 for t in targets if t in ("VIOLET", "GREEN"))
    hi_bit   = 1 if bo_votes > vg_votes else 0

    if hi_bit == 1:
        even_x_name, odd_x_name = "BLUE", "ORANGE"
    else:
        even_x_name, odd_x_name = "VIOLET", "GREEN"

    # Step 3: place bits. Renderer rules (matches Casso's
    # AppleHiResMode::Render and real Apple HW NTSC artefacting):
    #   - Pixel OFF                -> BLACK
    #   - Isolated ON, even abs x  -> phase A colour (violet or blue)
    #   - Isolated ON, odd  abs x  -> phase B colour (green or orange)
    #   - ON with an ON neighbour  -> WHITE
    # CRITICAL: the parity that matters is the ABSOLUTE pixel x, not
    # the bit index within the byte. For odd byte indices the two
    # parities flip, so a "violet" target at bit 0 of byte_idx=1
    # actually sits at x=7 (odd) and would render GREEN unless we
    # account for the flip here.
    bits         = 0
    base_x       = col_byte * PIXELS_PER_BYTE
    for bit, tgt in enumerate (targets):
        if tgt == "WHITE":
            bits |= (1 << bit)
        elif tgt == "BLACK":
            pass
        else:
            x_is_even = ((base_x + bit) & 1) == 0
            want_on   = ((tgt == even_x_name and x_is_even) or
                         (tgt == odd_x_name  and not x_is_even))
            if want_on:
                bits |= (1 << bit)
            # else: wrong-palette colour or wrong-phase position. Keep
            # OFF (black) instead of forcing ON, so the chromatic
            # pixels in this byte stay clean instead of being smeared
            # by white-artefact runs.

    return (hi_bit << 7) | bits


def image_to_hgr (img):
    if img.size != (HGR_WIDTH, HGR_HEIGHT):
        raise ValueError ("image must be 280x192 before bit-packing")

    rgb    = img.convert ("RGB")
    pixels = rgb.load ()
    out    = bytearray (HGR_BYTES)

    for row in range (HGR_HEIGHT):
        base = hgr_row_offset (row)
        for col_byte in range (ROW_BYTES):
            out[base + col_byte] = encode_byte (pixels, row, col_byte)

    return bytes (out)


def paint_title (canvas, text, top_y=4, font_size=18, stroke_width=0):
    # Paint a centered title across the top of the 280x192 canvas in
    # white. We try a few common Windows TrueType fonts at the
    # requested size; whatever we draw goes through the same colour
    # pipeline as the rest of the image, so crisp white text relies on
    # the encoder's WHITE classification (bright + desaturated) and
    # adjacent-ON artefacting.
    candidates = [
        "segoeui.ttf",
        "tahoma.ttf",
        "verdana.ttf",
        "arial.ttf",
        "calibri.ttf",
    ]
    font = None
    for name in candidates:
        try:
            font = ImageFont.truetype (name, font_size)
            break
        except OSError:
            continue
    if font is None:
        font = ImageFont.load_default ()

    draw = ImageDraw.Draw (canvas)
    bbox = draw.textbbox ((0, 0), text, font=font, stroke_width=stroke_width)
    text_w = bbox[2] - bbox[0]
    text_x = (HGR_WIDTH - text_w) // 2 - bbox[0]

    # Drop a black "shadow" behind the white text so that, even where
    # the title overlaps the cassowary's casque or a leaf, the glyphs
    # have at least one black neighbour each side and the encoder can
    # render them as solid white instead of WHITE-pixel-next-to-colour
    # which would just artefact garbage.
    pad = stroke_width + 1
    for dx in range (-pad, pad + 1):
        for dy in range (-pad, pad + 1):
            draw.text ((text_x + dx, top_y + dy), text,
                       font=font, fill=HGR_BLACK,
                       stroke_width=stroke_width, stroke_fill=HGR_BLACK)
    draw.text ((text_x, top_y), text,
               font=font, fill=HGR_WHITE,
               stroke_width=stroke_width, stroke_fill=HGR_WHITE)


def crop_and_fit (src_path, crop_box, letterbox, title=None,
                  title_size=18, title_stroke=0):
    img = Image.open (src_path).convert ("RGB")

    if crop_box is not None:
        img = img.crop (crop_box)

    src_w, src_h = img.size
    target_aspect = HGR_WIDTH / HGR_HEIGHT
    src_aspect    = src_w / src_h

    if letterbox:
        # Fit the cropped image entirely inside 280x192, padding the
        # short dimension with black. Preserves portrait subject
        # proportions (head + casque) at the cost of side bars.
        if src_aspect > target_aspect:
            new_w = HGR_WIDTH
            new_h = max (1, int (round (HGR_WIDTH / src_aspect)))
        else:
            new_h = HGR_HEIGHT
            new_w = max (1, int (round (HGR_HEIGHT * src_aspect)))

        scaled = img.resize ((new_w, new_h), Image.LANCZOS)
        canvas = Image.new ("RGB", (HGR_WIDTH, HGR_HEIGHT), HGR_BLACK)
        off_x  = (HGR_WIDTH  - new_w) // 2
        off_y  = (HGR_HEIGHT - new_h) // 2
        canvas.paste (scaled, (off_x, off_y))
        if title:
            paint_title (canvas, title, font_size=title_size, stroke_width=title_stroke)
        return canvas
    else:
        # Center-crop to target aspect, then resize. Loses content on the
        # long edge but fills the screen.
        if src_aspect > target_aspect:
            new_w   = int (src_h * target_aspect)
            crop_x  = (src_w - new_w) // 2
            img     = img.crop ((crop_x, 0, crop_x + new_w, src_h))
        else:
            new_h   = int (src_w / target_aspect)
            crop_y  = (src_h - new_h) // 2
            img     = img.crop ((0, crop_y, src_w, crop_y + new_h))

        canvas = img.resize ((HGR_WIDTH, HGR_HEIGHT), Image.LANCZOS)
        if title:
            paint_title (canvas, title, font_size=title_size, stroke_width=title_stroke)
        return canvas


def parse_crop (s):
    if s is None:
        return None
    parts = [p.strip () for p in s.split (",")]
    if len (parts) != 4:
        raise argparse.ArgumentTypeError (
            "crop must be 'left,top,right,bottom' (4 ints)")
    return tuple (int (p) for p in parts)


def main ():
    parser = argparse.ArgumentParser (description=__doc__)
    parser.add_argument ("--input",  required=True, help="source image path")
    parser.add_argument ("--output", required=True, help="raw HGR framebuffer output path")
    parser.add_argument ("--crop",   type=parse_crop, default=DEFAULT_CROP,
                         help="optional source-image crop box "
                              "'left,top,right,bottom' applied before fitting "
                              "(default: tuned for the standard cassowary "
                              "portrait under Assets/)")
    parser.add_argument ("--no-letterbox", action="store_true",
                         help="center-crop to fill the screen instead of "
                              "padding with side bars")
    parser.add_argument ("--preview", default=None,
                         help="if set, also write a PNG preview of the "
                              "post-fit image at this path (handy for "
                              "iterating on --crop)")
    parser.add_argument ("--title", default="Casso",
                         help="centered title text painted across the top "
                              "of the framebuffer (default: 'Casso'; pass "
                              "'' to disable)")
    parser.add_argument ("--title-size", type=int, default=18,
                         help="title font pixel size (default: 18)")
    parser.add_argument ("--title-stroke", type=int, default=0,
                         help="extra stroke pixels around each glyph "
                              "(default: 0; bump to 1 for a heavier look)")
    args = parser.parse_args ()

    src = Path (args.input)
    dst = Path (args.output)

    if not src.is_file ():
        print (f"error: source image not found: {src}", file=sys.stderr)
        return 1

    img = crop_and_fit (src, args.crop,
                        letterbox=not args.no_letterbox,
                        title=args.title or None,
                        title_size=args.title_size,
                        title_stroke=args.title_stroke)

    if args.preview:
        Path (args.preview).parent.mkdir (parents=True, exist_ok=True)
        img.save (args.preview)

    data = image_to_hgr (img)

    if len (data) != HGR_BYTES:
        print (f"error: expected {HGR_BYTES} bytes of HGR, got {len (data)}",
               file=sys.stderr)
        return 1

    dst.parent.mkdir (parents=True, exist_ok=True)
    dst.write_bytes (data)

    print (f"wrote {len (data)} bytes -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit (main ())
