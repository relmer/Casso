"""Apple UniDisk 3.5 (A2M2053): the intelligent SmartPort drive, in the
Apple 3.5 Drive's case (cad_floppy35.py) in the //c's Snow White."""

import os

from cad_floppy35 import build

SNOW_WHITE = (0.905, 0.890, 0.835)

m = build(SNOW_WHITE)

if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "UniDisk35.mesh"),
                    os.path.join(out, "UniDisk35.mtl"), "UniDisk35.mtl")
    print(f"UniDisk35 (CAD): {nv} verts, {nt} tris")
