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
#  Bit 7 picks the color palette pair: 0 = violet+green, 1 = blue+orange.
#  Within a byte, even-bit pixels (0,2,4,6) take the "phase A" color
#  (violet or blue), odd-bit pixels (1,3,5) take the "phase B" color
#  (green or orange). Two adjacent ON pixels appear as white due to NTSC
#  composite artifacting, two adjacent OFF pixels appear as black.
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

# LoRes (40x48 16-color graphics, sharing text-page-1 RAM at $0400-$07FF).
# 24 text rows; each row's byte packs two LoRes pixel rows as (high
# nibble << 4) | low nibble. Only the first 40 columns of each text
# row are visible -- the remaining 88 bytes per 128-byte chunk are
# "screen holes" and can be left as zero on disk.
LORES_PAGE_BYTES   = 1024
LORES_TEXT_ROWS    = 24
LORES_PIXEL_ROWS   = 48
LORES_VISIBLE_COLS = 40
LORES_COLORS       = 16

# DHGR (560x192 in monochrome, 140x192 in 16-color mode). Uses HGR
# page 1 on BOTH main and aux RAM, with aux supplying even-x byte
# columns and main supplying odd-x. The framebuffer for one half is
# the same 8 KB layout as standard HGR.
DHGR_COLORS        = 16

# Default source-image crop, picked to capture the cassowary's full
# casque + head + neck + wattles from the standard portrait photo
# (`Assets/3a Mrs Cassowary closeup 8167.jpg`, 880x1600). Pass --crop
# on the command line to override for other source images.
DEFAULT_CROP   = (60, 40, 860, 1100)


# Approximate sRGB values for the six Apple ][ HGR colors. Sourced from
# the standard NTSC HGR palette commonly used by emulators (AppleWin,
# Virtual ][), close to what real composite hardware produces.
HGR_BLACK   = (0,   0,   0  )
HGR_WHITE   = (255, 255, 255)
HGR_VIOLET  = (170, 30,  220)
HGR_GREEN   = (40,  220, 60 )
HGR_BLUE    = (30,  130, 255)
HGR_ORANGE  = (255, 130, 30 )

# For each palette (bit7=0 / bit7=1), per-pixel-phase colors:
#   phase A = even bit position in the byte (bits 0,2,4,6)
#   phase B = odd  bit position in the byte (bits 1,3,5)
PALETTE_VG = (HGR_VIOLET, HGR_GREEN)    # bit7 = 0
PALETTE_BO = (HGR_BLUE,   HGR_ORANGE)   # bit7 = 1


def hgr_row_offset (row):
    return (1024 * (row & 7)
          +  128 * ((row >> 3) & 7)
          +   40 * (row >> 6))


def color_distance_sq (a, b):
    dr = a[0] - b[0]
    dg = a[1] - b[1]
    db = a[2] - b[2]
    return dr * dr + dg * dg + db * db


def nearest_hgr_target (src):
    # Hue-based classification into the 6 HGR rendered colors. Plain
    # RGB Euclidean distance gets confused by desaturated colors --
    # e.g. the cassowary's brown casque (~140,90,80, R approximately
    # B) lands closer to HGR_VIOLET (170,30,220, lots of B) than to
    # HGR_ORANGE (255,130,30) by sRGB distance, so the casque was
    # rendering purple. Routing by which channel dominates (warm vs
    # cool, then green) recovers brown -> orange / blue-grey -> blue
    # without distorting the saturated colors.
    r, g, b = src
    luma    = (r + g + b) // 3
    chroma  = max (r, g, b) - min (r, g, b)

    # Hard neutral cases first.
    if luma >= 220 and chroma <= 30:
        return "WHITE"
    if luma <= 35:
        return "BLACK"
    if chroma <= 20:
        # Truly grey pixel. Tip to WHITE if bright, BLACK if dark.
        return "WHITE" if luma >= 140 else "BLACK"

    # Dominant-channel routing. Note "approximately equal" tolerances
    # so a pixel with R=B+8 still counts as warm rather than purple.
    eq_tol = 18

    if g > r + eq_tol and g > b + eq_tol:
        return "GREEN"

    warm = r > b + eq_tol           # R noticeably > B  -> warm hue
    cool = b > r + eq_tol           # B noticeably > R  -> cool hue
    purple = (not warm) and (not cool) and (r > g) and (b > g)

    if purple:
        return "VIOLET"
    if warm:
        # Warm: brown, red, orange. Only fall to VIOLET if the pixel
        # is genuinely magenta-ish (lots of red AND lots of blue with
        # green much lower).
        if b > g + eq_tol and r >= b - eq_tol:
            return "VIOLET"
        return "ORANGE"
    if cool:
        # Cool: any blue dominance ends up here. Pure violet is rare
        # in nature; only redirect if both R and B are high but G is
        # very low.
        if r > g + eq_tol and b > r - eq_tol:
            return "VIOLET"
        return "BLUE"

    # Fallback: green-ish desaturated.
    return "GREEN"


def encode_byte (pixels, row, col_byte):
    # Step 1: classify each of the 7 source pixels into an HGR target.
    targets = []
    for bit in range (PIXELS_PER_BYTE):
        x = col_byte * PIXELS_PER_BYTE + bit
        targets.append (nearest_hgr_target (pixels[x, row]))

    # Step 2: pick the palette pair (bit 7 of the byte). Weight blue/
    # orange votes a little higher than violet/green so that color
    # pixels of those palettes pull the byte even when they're
    # outnumbered by neutral neighbours -- otherwise a single blue
    # pixel surrounded by leaf-green ones loses the palette vote and
    # gets demoted to black or wrong-color artifact.
    bo_votes = sum (2 for t in targets if t in ("BLUE", "ORANGE"))
    vg_votes = sum (1 for t in targets if t in ("VIOLET", "GREEN"))
    hi_bit   = 1 if bo_votes > vg_votes else 0

    if hi_bit == 1:
        even_x_name, odd_x_name = "BLUE", "ORANGE"
    else:
        even_x_name, odd_x_name = "VIOLET", "GREEN"

    # Step 3: place bits. Renderer rules (matches Casso's
    # AppleHiResMode::Render and real Apple HW NTSC artifacting):
    #   - Pixel OFF                -> BLACK
    #   - Isolated ON, even abs x  -> phase A color (violet or blue)
    #   - Isolated ON, odd  abs x  -> phase B color (green or orange)
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
            # else: wrong-palette color or wrong-phase position. Keep
            # OFF (black) instead of forcing ON, so the chromatic
            # pixels in this byte stay clean instead of being smeared
            # by white-artifact runs.

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


def generate_color_bands_hgr ():
    # Synthesize an 8 KB HGR test pattern that exercises every NTSC
    # artifact color Casso's renderer can produce. Top-to-bottom:
    # 6 horizontal bands of 32 scanlines each, in order
    # BLACK / VIOLET / GREEN / WHITE / BLUE / ORANGE.
    #
    # Within each band, every byte is filled with a value that puts
    # ON pixels at exactly the right absolute-x parity for the target
    # color, accounting for the byte<->x parity flip on every odd
    # byte index (since 7 is odd, byte N starts at x=7N which
    # alternates parity).
    #
    # Diagnostic value: this is the canonical "if the renderer writes
    # bytes in the wrong layout, blue<->orange and violet<->green
    # show up swapped" test image. It rode on the demo disk for a
    # while so a keystroke could put it next to the cassowary, and
    # came back off once VideoModeTests started asking the same
    # question directly -- Render_SinglePixelPalette0EvenCol_Violet
    # and its neighbours assert per pixel what this asked the eye to
    # judge per band. Kept because it is still the fastest way to
    # SEE the answer; write it somewhere and BLOAD it.
    out = bytearray (HGR_BYTES)

    # (band name, even-byte value, odd-byte value).
    # For solid color stripes the rule is:
    #   target color at all absolute x of parity P -> ON when
    #   (N + b) parity = P, where N is the byte index in the row
    #   and b is the bit index within the byte. Because 7 is odd,
    #   even byte indices have b parity = x parity, odd byte
    #   indices have b parity = inverted x parity. So even-byte
    #   and odd-byte fills swap their bit masks.
    PATTERN_EVEN_X    = 0x55  # bits 0,2,4,6 -> even b
    PATTERN_ODD_X     = 0x2A  # bits 1,3,5   -> odd  b
    PATTERN_ALL_ON    = 0x7F  # adjacent ON -> renders as WHITE

    bands = [
        ("BLACK",  0x00,                       0x00                      ),
        ("VIOLET", PATTERN_EVEN_X,             PATTERN_ODD_X             ),
        ("GREEN",  PATTERN_ODD_X,              PATTERN_EVEN_X            ),
        ("WHITE",  PATTERN_ALL_ON,             PATTERN_ALL_ON            ),
        ("BLUE",   PATTERN_EVEN_X | 0x80,      PATTERN_ODD_X  | 0x80     ),
        ("ORANGE", PATTERN_ODD_X  | 0x80,      PATTERN_EVEN_X | 0x80     ),
    ]

    band_height = HGR_HEIGHT // len (bands)
    for band_idx, (_name, even_b, odd_b) in enumerate (bands):
        y0 = band_idx * band_height
        y1 = (band_idx + 1) * band_height if band_idx < len (bands) - 1 else HGR_HEIGHT
        for y in range (y0, y1):
            base = hgr_row_offset (y)
            for col in range (ROW_BYTES):
                out[base + col] = even_b if (col & 1) == 0 else odd_b

    return bytes (out)


def generate_lores_bars_lores ():
    # Synthesize the 1 KB text-page-1 ($0400-$07FF) content for a
    # LoRes 16-color bar test. The screen is partitioned into 16
    # vertical bands of ~12 LoRes scanlines (one per palette index
    # 0..15). Each text-screen byte holds two stacked LoRes pixel
    # rows packed as low / high nibble; we compute the byte for
    # each of the 24 text rows so the two LoRes rows it contains
    # sit at the correct band.
    #
    # Used by the bootable demo disk to verify the renderer's
    # 16-entry palette (NtscColorTable.h::kAppleColors) is
    # wired up correctly after the R/B byte-order fix.
    out = bytearray (LORES_PAGE_BYTES)

    for text_row in range (LORES_TEXT_ROWS):
        # The two LoRes pixel rows packed into this text row.
        top_row = 2 * text_row
        bot_row = 2 * text_row + 1
        top_c   = (top_row * LORES_COLORS) // LORES_PIXEL_ROWS
        bot_c   = (bot_row * LORES_COLORS) // LORES_PIXEL_ROWS
        byte    = ((bot_c & 0x0F) << 4) | (top_c & 0x0F)

        # Apple text-screen base for row R = $0400 + 128*(R%8) + 40*(R/8).
        # We're writing the page-relative offset (caller stamps the
        # whole 1 KB into $0400-$07FF verbatim).
        base = 128 * (text_row & 7) + 40 * (text_row >> 3)
        for col in range (LORES_VISIBLE_COLS):
            out[base + col] = byte

    return bytes (out)


def generate_dhgr_bands ():
    # Synthesize the 8 KB main + 8 KB aux pages of a DHGR 16-color
    # bar test. Each color gets a 12-scanline horizontal band, top
    # to bottom in palette index order 0..15. DHGR packs 7 pixels
    # per byte across an aux+main pair (aux holds even-x byte
    # columns, main holds odd-x; together one byte-pair covers 14
    # contiguous pixels), and 4 consecutive pixels make one 16-color
    # "cell". Dot j of a cell carries bit (j + 1) & 3 of the color, so
    # the LAST dot is the low bit -- the technote's order, and what
    # AppleDoubleHiResMode decodes. A solid stripe of color C wants
    # every pixel to read out as bit `((P + 1) % 4)` of C, where P is
    # the absolute pixel index.
    #
    # Used by the bootable demo disk to verify the DHGR 16-entry
    # palette (NtscColorTable.h::kAppleColors) is wired up
    # correctly after the R/B byte-order fix.
    aux  = bytearray (HGR_BYTES)
    main = bytearray (HGR_BYTES)

    rows_per_band = HGR_HEIGHT // DHGR_COLORS

    for row in range (HGR_HEIGHT):
        c = min (row // rows_per_band, DHGR_COLORS - 1)
        c_bits = [(c >> i) & 1 for i in range (4)]

        base = hgr_row_offset (row)
        for byte_pair in range (ROW_BYTES):
            # Pixel indices spanned by aux byte (P=14b..14b+6) and
            # main byte (P=14b+7..14b+13). LSB-first within each byte.
            aux_pixel0  = byte_pair * 14
            main_pixel0 = byte_pair * 14 + 7

            aux_byte  = 0
            main_byte = 0
            for bit in range (7):
                if c_bits[(aux_pixel0  + bit + 1) % 4]:
                    aux_byte  |= (1 << bit)
                if c_bits[(main_pixel0 + bit + 1) % 4]:
                    main_byte |= (1 << bit)

            aux [base + byte_pair] = aux_byte
            main[base + byte_pair] = main_byte

    return bytes (aux), bytes (main)


def paint_title (canvas, text, font_size=18, stroke_width=0, top_y=2):
    # Paint a centered title across the top of the canvas in white.
    # Canvas may be any width — the centering uses canvas.width, not
    # HGR_WIDTH, so this function works for both HGR (280) and DHGR
    # (560 dots / 280 letterbox / 140 color cells, depending on
    # caller). We try a few common Windows TrueType fonts at the
    # requested size; whatever we draw goes through the same color
    # pipeline as the rest of the image, so crisp white text relies
    # on the encoder's WHITE classification (bright + desaturated)
    # and adjacent-ON artifacting.
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
    text_x = (canvas.width - text_w) // 2 - bbox[0]

    # Drop a black "shadow" behind the white text so that, even where
    # the title overlaps the cassowary's casque or a leaf, the glyphs
    # have at least one black neighbour each side and the encoder can
    # render them as solid white instead of WHITE-pixel-next-to-color
    # which would just artifact garbage.
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
    parser.add_argument ("--input",  default=None,
                         help="source image path (omit when --pattern is given)")
    parser.add_argument ("--output", required=True, help="raw HGR framebuffer output path")
    parser.add_argument ("--pattern",
                         choices=("bands", "lores-bars", "dhgr-bands-aux", "dhgr-bands-main"),
                         default=None,
                         help="emit a synthetic test pattern instead of "
                              "encoding an image. Options: 'bands' (HGR "
                              "6-color stripes, 8 KB), 'lores-bars' "
                              "(LoRes 16-color stripes packed into text "
                              "page 1, 1 KB), 'dhgr-bands-aux' / "
                              "'dhgr-bands-main' (DHGR 16-color stripes, "
                              "8 KB each side; ship both onto separate "
                              "disk tracks then RAMWRT-toggle when "
                              "loading)")
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

    dst = Path (args.output)

    if args.pattern == "bands":
        data = generate_color_bands_hgr ()
    elif args.pattern == "lores-bars":
        data = generate_lores_bars_lores ()
    elif args.pattern == "dhgr-bands-aux":
        data, _ = generate_dhgr_bands ()
    elif args.pattern == "dhgr-bands-main":
        _, data = generate_dhgr_bands ()
    else:
        if args.input is None:
            print ("error: either --input or --pattern is required",
                   file=sys.stderr)
            return 1

        src = Path (args.input)
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

    # Size check varies by pattern.
    expected_size = None
    if args.pattern == "lores-bars":
        expected_size = LORES_PAGE_BYTES
    elif args.pattern in (None, "bands", "dhgr-bands-aux", "dhgr-bands-main"):
        expected_size = HGR_BYTES

    if expected_size is not None and len (data) != expected_size:
        print (f"error: expected {expected_size} bytes, got {len (data)}",
               file=sys.stderr)
        return 1

    dst.parent.mkdir (parents=True, exist_ok=True)
    dst.write_bytes (data)

    print (f"wrote {len (data)} bytes -> {dst}")
    return 0


if __name__ == "__main__":
    sys.exit (main ())
