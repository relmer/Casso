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
| `cad_diskii.py` | `Resources/Models/DiskII/` | Apple Disk II A2M0003 (155×220×96 mm) |
| `cad_disk2c.py` | `Resources/Models/Disk2c/` | Apple //c external Drive A2M4020 (152×216×70) |
| `cad_monitor2c.py` | `Resources/Models/Monitor2c/` | Apple Monitor //c G090H (248×280×200) |
| `cad_monitor2.py` | `Resources/Models/Monitor2/` | Apple Monitor II A2M2010 (343×348×292) |
| `gen_duodisk.py` | `Resources/Models/DuoDisk/` | Apple DuoDisk A9M0106 (386×89×222) — meshkit, not yet migrated |
| `gen_profile.py` | `Resources/Models/ProFile/` | Apple ProFile 5 MB (439×110×226) — meshkit, not yet migrated |

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
python gen_diskii.py                                   # writes DiskII.mesh/.mtl here
python preview.py DiskII.mesh DiskII.mtl out.png 14 -60 # elev, azim
```

`preview.py` is a small software z-buffer renderer (PIL + numpy) — the
painter's algorithm cannot resolve the interpenetrating CSG-style solids
these generators emit, but a depth buffer (like the real D3D renderer's)
resolves them exactly.

After regenerating, copy the OBJ/MTL pair over the checked-in copy under
`Resources/Models/<Name>/`. Nothing else is needed: the Casso build runs
`MeshBake` over every model there and embeds the baked result.

**The app never parses this text.** It reads `MeshBake`'s output, which is the
same triangle list packed with shared positions and one color per material.
The Monitor II is 1.3 million lines of OBJ, and re-reading them at every launch
was most of Casso's startup, 15.6 seconds of a 17.7 second Debug start. Adding
a model means naming its directory in the `BakedMesh` list in
`Casso/Casso.vcxproj` and adding its `RCDATA` line to `Casso.rc`.

## CAD models

`cadkit.py` bridges [CadQuery](https://cadquery.readthedocs.io/) solids to the
same OBJ/MTL dialect above, one `usemtl` group per part, so a CAD-built model
drops into `Resources/Models/` unchanged. Everything the desk scene loads is CAD-built now; `gen_*` remains only for the not-yet-shipped DuoDisk and ProFile.

Prefer it over hand-emitting geometry. A wall of `add_quad` calls cannot be
reviewed: nothing in the source makes *"this face covers the whole screen
opening"* visible, so shape errors only surface in a screenshot. With a kernel
an opening is a boolean cut — it cannot silently be a solid face — and a
softened edge is a fillet rather than a hand-built chamfer strip.

```powershell
python -m pip install cadquery
python cad_monitor2.py                                     # writes Monitor2.mesh/.mtl here
python preview.py Monitor2.mesh Monitor2.mtl out.png 8 -90  # LOOK at it (elev, azim)
```

**Render the preview before wiring a model into the app.** Two of the desk
scene's model regressions — a case front that hid the entire screen, and a
tube curled tight enough that the raster read as a disc — were visible in a
one-second preview and reached the user instead.

Proportions are worth measuring off a reference photograph as fractions of the
overall case, rather than picked to feel right; `cad_monitor2.py` records the
ones it was built from.
