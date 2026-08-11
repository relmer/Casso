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
