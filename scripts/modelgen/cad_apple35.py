"""Apple 3.5 Drive (A9M0106): the 800K drive of the Apple II 3.5 Disk
Controller Card and the IIGS, in the shared case (cad_floppy35.py) in
Platinum. The desk draws the //c Plus's built-in drive with this model too.
"""

import os

from cad_floppy35 import build

PLATINUM = (0.835, 0.815, 0.760)

# The seam two thirds of the way up, front and back, and the slot right
# under it, as the iFixit teardown's photographs of the drive have them.
SEAM_FRAC = 0.68

name = "Apple35Drive"
m    = build(PLATINUM, seam_frac=SEAM_FRAC, slot_top_frac=SEAM_FRAC)

if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, name + ".mesh"),
                    os.path.join(out, name + ".mtl"), name + ".mtl")
    print(f"{name} (CAD): {nv} verts, {nt} tris")
