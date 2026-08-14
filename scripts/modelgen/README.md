# modelgen — parametric device models for the 3D desk scene

Generates the CAD models under `Resources/Models/` as Wavefront OBJ/MTL in
the exact dialect `ObjMeshParser` consumes (the same subset Tinkercad
exports: millimeter units, global vertex indices, fan-triangulated faces,
flat `Kd` materials — no normals, UVs, or textures).

Every model is a small Python program over `meshkit.py` (boxes, tapered
hexahedra, cylinders, spherical-sag panels), so dimensions and details are
named constants — "slot 4 mm lower" is a one-line change and a re-render,
and a Tinkercad-refined model can replace any generated one at any time
without touching the loader.

| Generator | Output | Object |
|---|---|---|
| `gen_diskii.py` | `Resources/Models/DiskII/` | Apple Disk II (155×86×222 mm) |
| `gen_duodisk.py` | `Resources/Models/DuoDisk/` | Apple DuoDisk A9M0106 (386×89×222) |
| `gen_disk2c.py` | `Resources/Models/Disk2c/` | Apple //c external Drive A2M4020 (152×70×216) |
| `gen_profile.py` | `Resources/Models/ProFile/` | Apple ProFile 5 MB (439×110×226) |
| `gen_monitor2c.py` | `Resources/Models/Monitor2c/` | Apple Monitor //c (248×200×280 + stand) |

**Color is identity.** The desk scene splits a model into sub-meshes by
matching `Kd` VALUES within ±0.02 per channel (`DeskSceneModel::kKdEpsilon`)
against the palette in `DeskSceneModel.h` — glass, monitor lamp, drive lamp,
door, latch. A color that drifts inside that epsilon of one of them is swept
into that sub-mesh with no diagnostic, and for glass that means *dropped*
(the scene generates its own tube). Keep every other color at least a channel
clear of the palette.

The Monitor //c's glass is a true spherical-sag mesh — the surface the
live emulator framebuffer will texture onto in the desk scene, with the
sag radius as a named parameter.

## Usage

```powershell
cd scripts/modelgen
python gen_diskii.py                                   # writes DiskII.obj/.mtl here
python preview.py DiskII.obj DiskII.mtl out.png 14 -60 # elev, azim
```

`preview.py` is a small software z-buffer renderer (PIL + numpy) — the
painter's algorithm cannot resolve the interpenetrating CSG-style solids
these generators emit, but a depth buffer (like the real D3D renderer's)
resolves them exactly.

After regenerating, copy the OBJ/MTL pair over the checked-in copy under
`Resources/Models/<Name>/`.

## CAD models

`cadkit.py` bridges [CadQuery](https://cadquery.readthedocs.io/) solids to the
same OBJ/MTL dialect above, one `usemtl` group per part, so a CAD-built model
drops into `Resources/Models/` unchanged. `cad_monitor2.py` is the first.

Prefer it over hand-emitting geometry. A wall of `add_quad` calls cannot be
reviewed: nothing in the source makes *"this face covers the whole screen
opening"* visible, so shape errors only surface in a screenshot. With a kernel
an opening is a boolean cut — it cannot silently be a solid face — and a
softened edge is a fillet rather than a hand-built chamfer strip.

```powershell
python -m pip install cadquery
python cad_monitor2.py                                     # writes Monitor2.obj/.mtl here
python preview.py Monitor2.obj Monitor2.mtl out.png 8 -90  # LOOK at it (elev, azim)
```

**Render the preview before wiring a model into the app.** Two of the desk
scene's model regressions — a case front that hid the entire screen, and a
tube curled tight enough that the raster read as a disc — were visible in a
one-second preview and reached the user instead.

Proportions are worth measuring off a reference photograph as fractions of the
overall case, rather than picked to feel right; `cad_monitor2.py` records the
ones it was built from.
