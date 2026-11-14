"""Apple 3.5 Drive (A9M0106): the platinum 800K drive of the Apple II 3.5 Disk
Controller Card and the IIGS, narrow and deep.

THE DIMENSIONS: 4 3/4 in wide, 2 1/4 in high, 8 1/4 in deep, from the
Computer History Museum's catalog entry 102735399. The front's layout is not
measured from a photograph; for the owner's review (T108).

`--plus` writes the //c Plus internal drive's variant in the //c Plus's
lighter platinum, as Apple35Plus.mesh.
"""

import os
import sys

from cad_floppy35 import INCH, build

W, D, H = 4.75 * INCH, 8.25 * INCH, 2.25 * INCH

PLATINUM      = (0.835, 0.815, 0.760)
PLATINUM_BEZ  = (0.780, 0.760, 0.705)
PLUS          = (0.880, 0.870, 0.835)
PLUS_BEZ      = (0.830, 0.820, 0.785)

plus  = "--plus" in sys.argv
name  = "Apple35Plus" if plus else "Apple35Drive"
m     = build(W, D, H, PLUS if plus else PLATINUM, PLUS_BEZ if plus else PLATINUM_BEZ,
              slot_w = 92.0, slot_z = 34.0)

if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, name + ".mesh"),
                    os.path.join(out, name + ".mtl"), name + ".mtl")
    print(f"{name} (CAD): {nv} verts, {nt} tris")
