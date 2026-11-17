"""The rainbow Apple badge, read from Casso's branding bitmap so every model
shares one silhouette and one set of stripe colors. badge() returns the parts
to add, one per stripe, upright on the front face (y = 0) with its lower left
corner at (x0, z0) and `height` mm tall."""

import io
import os
import re

import cadquery as cq


def read_branding():
    src = io.open(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "..", "..", "CassoEmuCore", "Ui", "Chrome",
                               "CassoBranding.cpp"), encoding="utf-8").read()

    sil_body = re.search(r"s_kSilhouette\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
    rows     = [int(v, 16) for v in re.findall(r"0x([0-9A-Fa-f]+)ULL", sil_body)]

    col_body = re.search(r"s_kStripeColors\[[^\]]*\]\s*=\s*\{(.*?)\};", src, re.S).group(1)
    stripes  = [((int(v, 16) >> 16 & 0xFF) / 255.0,
                 (int(v, 16) >> 8 & 0xFF) / 255.0,
                 (int(v, 16) & 0xFF) / 255.0)
                for v in re.findall(r"0x[Ff]{2}([0-9A-Fa-f]{6})", col_body)]

    return rows, stripes


def badge_ink(height):
    # The inked area of a badge `height` mm tall, as (x offset, z offset,
    # width, height) from the corner badge() places, for centering it.
    rows, _ = read_branding()
    cell    = height / len(rows)
    nonzero = [i for i, r in enumerate(rows) if r]
    cols    = [c for r in rows for c in range(64) if (r >> c) & 1]

    return (min(cols) * cell, (len(rows) - nonzero[-1] - 1) * cell,
            (max(cols) + 1 - min(cols)) * cell, (nonzero[-1] - nonzero[0] + 1) * cell)


def badge(model, x0, z0, height, y = -0.3, prefix = "badge", color = None, place = None):
    # `color` makes the badge one color instead of stripes; `place` maps each
    # stripe's compound before it's added, for a badge on the back.
    rows, stripes = read_branding()
    cell          = height / len(rows)
    nonzero       = [i for i, r in enumerate(rows) if r]
    first, last   = nonzero[0], nonzero[-1]
    by_stripe     = {}

    for row, bits in enumerate(rows):
        if not bits:
            continue

        banded = min(last, max(first, row))
        stripe = ((banded - first) * len(stripes)) // (last - first + 1)
        rz     = z0 + (len(rows) - row - 1) * cell
        col    = 0

        while col < 64:
            if not (bits >> col) & 1:
                col += 1
                continue

            run = col
            while run < 64 and (bits >> run) & 1:
                run += 1

            by_stripe.setdefault(stripe, []).append(
                cq.Workplane("XY")
                  .box((run - col) * cell, 0.5, cell + 0.02, centered=(False, False, False))
                  .translate((x0 + col * cell, y, rz))
                  .val())
            col = run

    for stripe, solids in sorted(by_stripe.items()):
        part = cq.Compound.makeCompound(solids)

        if place is not None:
            part = place(part)

        model.add(f"{prefix}{stripe}", part, color if color is not None else stripes[stripe])
