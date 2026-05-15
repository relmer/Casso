#!/usr/bin/env python3
"""
Generate Apple //e DHGR (Double Hi-Res) 16-color bar test patterns.

Output:
  Apple2/Demos/dhgr-bars-aux.bin   (8 KB — DHGR aux RAM bytes for $2000-$3FFF)
  Apple2/Demos/dhgr-bars-main.bin  (8 KB — DHGR main RAM bytes for $2000-$3FFF)

DHGR layout reminder:
  - 560 dots wide x 192 rows, monochrome representation; 4 dots = 1 nibble,
    LSB-first along the row, selecting one of 16 colors
  - 80 bytes per scanline, interleaved: aux[0] main[0] aux[1] main[1] ...
  - 7 dots packed per byte, bit 0 = leftmost, bit 7 ignored
  - Row offset uses the same HGR formula:
      base = 1024*(R&7) + 128*((R>>3)&7) + 40*(R>>6)

Pattern: 16 vertical color bars, each 35 dots wide. Each 4-dot group is
nibbled with the color of the bar containing the group's center dot.
"""

import struct
import sys
from pathlib import Path

ROW_HEIGHT  = 192
DOT_WIDTH   = 560
ROW_BYTES   = 40            # per side (aux or main)
NUM_COLORS  = 16


def hgr_row_offset(row: int) -> int:
    return 1024 * (row & 7) + 128 * ((row >> 3) & 7) + 40 * (row >> 6)


def color_at_dot(d: int) -> int:
    """Return the 4-bit color index for dot d. The center of d's 4-dot group
    selects which of the 16 vertical bars the dot belongs to."""
    group_start = (d // 4) * 4
    group_center = group_start + 2
    bar = (group_center * NUM_COLORS) // DOT_WIDTH
    return min(bar, NUM_COLORS - 1)


def build_row() -> tuple[bytes, bytes]:
    aux = bytearray(ROW_BYTES)
    main = bytearray(ROW_BYTES)
    for d in range(DOT_WIDTH):
        nibble = color_at_dot(d)
        bit_in_nibble = d - ((d // 4) * 4)
        bit_value = (nibble >> bit_in_nibble) & 1
        if bit_value == 0:
            continue
        byte_idx = d // 7
        bit_idx  = d - byte_idx * 7
        target = aux if (byte_idx % 2) == 0 else main
        sub_idx = byte_idx // 2
        target[sub_idx] |= (1 << bit_idx)
    return bytes(aux), bytes(main)


def main():
    aux_row, main_row = build_row()

    aux_buf  = bytearray(8192)
    main_buf = bytearray(8192)
    for row in range(ROW_HEIGHT):
        base = hgr_row_offset(row)
        aux_buf[base:base + ROW_BYTES]  = aux_row
        main_buf[base:base + ROW_BYTES] = main_row

    out_dir = Path(__file__).resolve().parent.parent / "Apple2" / "Demos"
    aux_path  = out_dir / "dhgr-bars-aux.bin"
    main_path = out_dir / "dhgr-bars-main.bin"
    aux_path.write_bytes(bytes(aux_buf))
    main_path.write_bytes(bytes(main_buf))
    print(f"wrote {aux_path} ({len(aux_buf)} bytes)")
    print(f"wrote {main_path} ({len(main_buf)} bytes)")


if __name__ == "__main__":
    sys.exit(main())
