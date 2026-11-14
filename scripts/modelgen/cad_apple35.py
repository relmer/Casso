"""Apple 3.5 Drive (A9M0106): the 800K drive of the Apple II 3.5 Disk
Controller Card and the IIGS, in the shared case (cad_floppy35.py) in
Platinum.

`--plus` writes the //c Plus's built-in drive, drawn as an Apple 3.5 Drive in
the //c Plus's lighter platinum, as Apple35Plus.mesh.
"""

import os
import sys

from cad_floppy35 import build

PLATINUM = (0.835, 0.815, 0.760)
PLUS     = (0.880, 0.870, 0.835)

plus = "--plus" in sys.argv
name = "Apple35Plus" if plus else "Apple35Drive"
m    = build(PLUS if plus else PLATINUM)

if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, name + ".mesh"),
                    os.path.join(out, name + ".mtl"), name + ".mtl")
    print(f"{name} (CAD): {nv} verts, {nt} tris")
