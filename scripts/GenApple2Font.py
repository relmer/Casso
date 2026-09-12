"""Build the Apple II text face from the character generator already in the tree.

    Resources/Fonts/CassoApple2.ttf

The glyphs are READ OUT OF CassoEmuCore/Machines/Apple2/Common/CharacterRom.h,
never copied into this script: the emulator draws its 40-column text from that
table, and a second copy of the dots would let the preview and the screen drift
apart. Change the table and rerun this; nothing else knows the shapes.

The table declares room for 96 characters but carries 64 of them, $20 through
$5F: space, punctuation, digits and uppercase, eight bytes each, one byte per
row, bit 0 the leftmost of seven dots. Each row's lit dots become rectangles --
runs merged along the row, so a stroke is one contour rather than a line of
squares meeting at their edges, which is what leaves seams at small sizes.

The cell is seven dots wide and eight tall, and the advance is the cell: the
Apple II's text is a grid, and a face that keeps that grid is what makes a
listing line up the way it did on the machine. The baseline sits one row above
the cell's bottom, so the comma and the semicolon hang below it as they do in
the ROM.

The II and II+ had no lowercase, and their character generator decoded six
bits: $60 through $7F drew the same shapes as $40 through $5F. So a through z
point at A through Z here, and so do the few punctuation marks above $5F --
not as a substitute for glyphs we lack, but because that is what the machine
put on the screen for those codes.

Needs fontTools:  pip install fonttools

Run from the repository root:

    python scripts/GenApple2Font.py
"""

import re
import sys
from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen


HERE = Path(__file__).resolve().parent
REPO = HERE.parent
CHAR_ROM = REPO / "CassoEmuCore" / "Machines" / "Apple2" / "Common" / "CharacterRom.h"
OUT_TTF = REPO / "Resources" / "Fonts" / "CassoApple2.ttf"

FIRST_CODE = 0x20          # the table starts at space
GLYPH_ROWS = 8             # bytes per character
GLYPH_DOTS = 7             # dots per row, bit 0 leftmost

UPM = 1000
DOT = 125                  # 8 rows * 125 = 1000 units per em
BASELINE_ROWS = 1          # rows below the baseline, for the comma's tail

ASCENT = (GLYPH_ROWS - BASELINE_ROWS) * DOT
DESCENT = BASELINE_ROWS * DOT
ADVANCE = GLYPH_DOTS * DOT

FAMILY = "Casso Apple II"
VERSION = "1.000"


def read_char_rom():
    """The 768 bytes of kApple2CharRom, in the header's order."""
    text = CHAR_ROM.read_text(encoding="utf-8", errors="replace")
    match = re.search(r"kApple2CharRom\s*\[\s*(\d+)\s*\]\s*=\s*\{(.*?)\};", text, re.S)
    if not match:
        raise SystemExit(f"no kApple2CharRom table in {CHAR_ROM}")

    declared = int(match.group(1))
    body = re.sub(r"//[^\n]*", "", match.group(2))
    values = [int(v, 16) for v in re.findall(r"0[xX]([0-9A-Fa-f]{1,2})\b", body)]

    #  The table is declared with room to spare and written out as far as the
    #  shapes go; C zeroes the rest, and a blank glyph is not a glyph.
    if len(values) > declared:
        raise SystemExit(f"table declares {declared} bytes, holds {len(values)}")
    if len(values) % GLYPH_ROWS:
        raise SystemExit(f"{len(values)} bytes is not a whole number of {GLYPH_ROWS}-row glyphs")

    return values


def row_runs(rowByte):
    """The lit spans of one row, as (firstDot, dotCount) left to right."""
    runs = []
    dot = 0
    while dot < GLYPH_DOTS:
        if rowByte & (1 << dot):
            start = dot
            while dot < GLYPH_DOTS and (rowByte & (1 << dot)):
                dot += 1
            runs.append((start, dot - start))
        else:
            dot += 1
    return runs


def draw_glyph(rows):
    """One glyph: every run of lit dots as a rectangle, wound clockwise."""
    pen = TTGlyphPen(None)

    for index, rowByte in enumerate(rows):
        # Row 0 is the top of the cell; y counts up from the baseline.
        top = ASCENT - index * DOT
        bottom = top - DOT

        for first, count in row_runs(rowByte):
            left = first * DOT
            right = left + count * DOT

            pen.moveTo((left, bottom))
            pen.lineTo((left, top))
            pen.lineTo((right, top))
            pen.lineTo((right, bottom))
            pen.closePath()

    return pen.glyph()


def glyph_name(code):
    return f"uni{code:04X}"


def main() -> None:
    rom = read_char_rom()
    count = len(rom) // GLYPH_ROWS

    glyphs = {".notdef": TTGlyphPen(None).glyph()}
    metrics = {".notdef": (ADVANCE, 0)}
    cmap = {}

    for index in range(count):
        code = FIRST_CODE + index
        name = glyph_name(code)
        rows = rom[index * GLYPH_ROWS:(index + 1) * GLYPH_ROWS]

        glyphs[name] = draw_glyph(rows)
        metrics[name] = (ADVANCE, 0)
        cmap[code] = name

    #  The character generator decoded six bits, so $60-$7F drew what $40-$5F
    #  draws: lowercase came out as uppercase, and so did the punctuation
    #  above it.
    for code in range(0x60, 0x80):
        folded = code - 0x20
        if folded in cmap:
            cmap[code] = cmap[folded]

    order = [".notdef"] + [glyph_name(FIRST_CODE + i) for i in range(count)]

    fb = FontBuilder(UPM, isTTF=True)
    fb.setupGlyphOrder(order)
    fb.setupCharacterMap(cmap)
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics(metrics)
    fb.setupHorizontalHeader(ascent=ASCENT, descent=-DESCENT)
    fb.setupNameTable({
        "familyName":   FAMILY,
        "styleName":    "Regular",
        "uniqueFontIdentifier": f"{FAMILY} {VERSION}",
        "fullName":     FAMILY,
        "psName":       FAMILY.replace(" ", ""),
        "version":      VERSION,
        "copyright":    "Glyph shapes from the Apple II character generator table in CassoEmuCore.",
    })
    #  Panose wants every field; 2 is a text face and 9 its monospaced
    #  proportion, which is what tells a picker this face is fixed-width.
    panose = {name: 0 for name in ("bFamilyType", "bSerifStyle", "bWeight", "bProportion", "bContrast",
                                   "bStrokeVariation", "bArmStyle", "bLetterForm", "bMidline", "bXHeight")}
    panose["bFamilyType"] = 2
    panose["bProportion"] = 9

    fb.setupOS2(sTypoAscender=ASCENT, sTypoDescender=-DESCENT, usWinAscent=ASCENT, usWinDescent=DESCENT,
                panose=panose)
    fb.setupPost(isFixedPitch=1)

    OUT_TTF.parent.mkdir(parents=True, exist_ok=True)
    fb.save(OUT_TTF)

    print(f"  {OUT_TTF.relative_to(REPO)}  {count} glyphs, {OUT_TTF.stat().st_size} B")


if __name__ == "__main__":
    sys.exit(main())
