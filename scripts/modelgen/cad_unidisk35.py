"""Apple UniDisk 3.5 (A2M2053): the intelligent SmartPort drive in the //c's
snow white, wider than it is tall, with grooves across the front below the
slot.

THE DIMENSIONS: about 175 mm wide, 50 mm high and 200 mm deep, from the
retrotechcollection wiki's 200 x 175 x 50 mm, read as the case's depth,
width and height. Not measured from a photograph; for the owner's review
(T108).
"""

import os

from cad_floppy35 import build

W, D, H = 175.0, 200.0, 50.0

CASE  = (0.905, 0.895, 0.860)   # snow white
BEZEL = (0.860, 0.850, 0.815)

m = build(W, D, H, CASE, BEZEL, slot_w = 96.0, slot_z = 30.0, grooves = 4)

if __name__ == "__main__":
    out = os.path.dirname(os.path.abspath(__file__))
    nv, nt = m.emit(os.path.join(out, "UniDisk35.mesh"),
                    os.path.join(out, "UniDisk35.mtl"), "UniDisk35.mtl")
    print(f"UniDisk35 (CAD): {nv} verts, {nt} tris")
