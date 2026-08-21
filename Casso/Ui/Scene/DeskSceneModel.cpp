#include "Pch.h"

#include "Ui/Scene/DeskSceneModel.h"

#include "Devices/Printer/ObjMeshParser.h"
#include "Ui/Chrome/CassoBranding.h"
#include "Ui/Chrome/DriveWidget.h"





// The room's two ceiling lights, in DESK space: origin at the computer's
// center on the desk surface, X right, Y back (toward the wall), Z up. The
// ceiling hangs five feet above the desk; one fixture is two feet left of
// the computer, the other seven feet right, both slightly forward of the
// machines so the front faces catch some direct light. Load() translates
// these into each model's own coordinates.
static constexpr float   s_kRoomLights[2][3] =
{
    { -610.0f, -450.0f, 1524.0f },     // 2 ft left, 5 ft up
    { 2134.0f, -450.0f, 1524.0f },     // 7 ft right, 5 ft up
};

// Falloff reference and the shading ramp now live on the class, in the
// header: the pixel shader applies them and the scene passes them along.

// Where the monitor's model-space floor rests in desk space: on top of the
// drive stack. A nominal Disk II height serves both pairings -- at five
// feet of throw the shorter //c drives move the light angle by well under
// a degree.
static constexpr float   s_kMonitorRestMm = 96.0f;

// The lamp as a REAL emitter, baked at load into a second copy of the body.
// A glow disc drawn over the lens cannot know the lamp sits at the back of
// a pocket: only tracing the light and letting the notch's own walls block
// it puts the spill where the housing allows it. The range bounds both which
// faces receive light and which triangles are tested as occluders, which is
// what keeps the shadow rays cheap -- past it the inverse square has taken
// the contribution below a display step anyway.

// Brand stamp placement on the monitor chin (model mm): the cassowary spans
// this box, proud of the bezel plate's front face (y = -10), inside the
// slimmed chin band (bezel z 9 .. 29).
static constexpr float   s_kBrandLeftMm   = 24.0f;
static constexpr float   s_kBrandTopZMm   = 27.0f;
static constexpr float   s_kBrandHeightMm = 16.0f;
static constexpr float   s_kBrandFrontY   = -10.6f;

// The Monitor II's mark: low on the right reveal, on the axis the MODEL
// names through its brand anchor -- the reveal's center line, which is the
// power notch's too, so mark, molded icon, and button share one column.
// The left edge is then solved at load from that axis: the cassowary's
// drawn mass sits off-center inside its 36-column grid, so centering the
// stamp's box mis-centers the visual weight, and the silhouette's mass
// centroid gives the exact correction. The //c chin and drive placements
// bake their bias into tuned constants instead.
//
// COUPLED TO THE GENERATOR. cad_monitor2.py cuts a rounded-corner pocket
// BRAND_D deep for this mark (see its BRAND_* block, which quotes the
// silhouette's drawn bounds). The stamp stands in that pocket at the same
// thickness, so its face finishes FLUSH with the frame -- front at y = 0,
// back on the pocket floor. Change either side and the other has to follow.
//
// BRAND_D tracks the icons' RIDGE_H by design -- the badge is as thick as the
// icons are proud. The hazard is not the coupling, it is that THIS side is a
// literal and cannot follow: thin the relief in the generator alone and the
// pocket shrinks while the badge does not, leaving it standing proud of the
// frame it is supposed to finish flush with.
static constexpr float   s_kMon2BrandTopZMm   = 46.0f;
static constexpr float   s_kMon2BrandHeightMm = 24.0f;

// Standing IN the pocket at its full depth, so the mark's face finishes flush
// with the frame. BuildBrandSolid gives it the smoothed outline and rolled top
// edge; laid flat on the pocket floor instead, it read as artwork in a tray
// rather than a badge set into the case.
static constexpr float   s_kMon2BrandThickMm  = 0.5f;    // == the CAD's BRAND_D
static constexpr float   s_kMon2BrandFrontY   = 0.0f;

// How the mark sits left-to-right inside its recess. 0 puts its drawn MASS on
// the pocket's center line, 1 centers its bounding BOX there instead. The two
// differ by about 1.5 mm, because the cassowary carries its weight left of its
// own box -- the tail reaches right while the body sits left.
//
// Mass wins. The eye centers a shape on where its weight is, not on the empty
// rectangle that circumscribes it, and box-centering visibly pushed the bird
// right in its panel. This is the same reasoning that put the mark on the
// strip axis by centroid before there was a panel around it at all.
static constexpr float   s_kMon2BrandBoxCenter = 0.0f;

// The drive's cassowary, lower-right of the faceplate like the 2D widget,
// proud of the black plate (front y = -1).
static constexpr float   s_kDriveBrandLeftMm   = 127.0f;
static constexpr float   s_kDriveBrandTopZMm   = 38.0f;
static constexpr float   s_kDriveBrandHeightMm = 29.0f;
static constexpr float   s_kDriveBrandFrontY   = -1.8f;

// The IN-USE label: "IN USE" plus the pointer triangle, sitting to the
// LED's left at the LED's height (the 2D widget's arrangement).
// Shares the drive label's left edge -- the outer edge of the slot frame's
// left bevel. See kLabelLeftMm in DeskScene.cpp; the value is
// FRAME_X0 + FRAME_FLAT from cad_diskii.py.
static constexpr float   s_kInUseLeftMm  = 12.23f;
static constexpr float   s_kInUseTopZMm  = 31.1f;
// 0.5 at the old 5x7 grid. The legend keeps its measured 24 mm width: the
// advance went 6 cells -> 8, so the cell shrinks to match rather than the
// label growing.
static constexpr float   s_kInUseCellMm  = 0.375f;
static constexpr float   s_kInUseFrontY  = -1.8f;
static constexpr float   s_kInUseRgb[3]  = { 0.750f, 0.730f, 0.700f };

// The "disk ][" logotype, low on the FACEPLATE -- below the IN-USE legend and
// its lamp, opposite the cassowary in the other corner. It was briefly put on
// the lid, which was simply wrong about where the mark lives.
//
// Shares the left column the other two legends use, so the face reads as one
// left-aligned stack rather than three separately-placed marks.
static constexpr float   s_kWordLeftMm   = 12.23f;
static constexpr float   s_kWordTopZMm   = 22.0f;
static constexpr float   s_kWordCellMm   = 0.52f;   // -> a 40 mm logotype
static constexpr float   s_kWordFrontY   = -1.8f;

// Printed ink, not molded plastic. Slightly softer than the legends' gray so
// the logotype does not out-shout the labels that carry actual information.
static constexpr float   s_kWordRgb[3]   = { 0.815f, 0.800f, 0.780f };

// DiskII interactive regions, model space (mm). The eject region wraps the
// slot + door bar + latch; the body box wraps the whole case including the
// proud front furniture.
static constexpr float   s_kDiskIiEjectMin[3] = {  14.0f, -5.0f, 49.1f };
static constexpr float   s_kDiskIiEjectMax[3] = { 141.0f,  3.0f, 70.3f };
static constexpr float   s_kDiskIiBodyMin[3]  = {   0.0f, -5.0f,  0.0f };
static constexpr float   s_kDiskIiBodyMax[3]  = { 155.0f, 220.0f, 96.0f };

// Write-protect padlock stamp on the drive faceplate (model mm): brass body
// with a shackle arch and a keyhole, top-right beside the badge row like
// the 2D widget's badge. Flat proud quads like the brand stamp; each layer
// floats a hair nearer the viewer than the one it sits on so depth never
// ties.
static constexpr float   s_kPadlockBodyX0    = 127.0f;
static constexpr float   s_kPadlockBodyX1    = 137.0f;
static constexpr float   s_kPadlockBodyZ0    = 64.7f;
static constexpr float   s_kPadlockBodyZ1    = 74.2f;
static constexpr float   s_kPadlockArchZ1    = 80.4f;
static constexpr float   s_kPadlockLegW      = 1.7f;
static constexpr float   s_kPadlockArchInset = 1.5f;
static constexpr float   s_kPadlockHoleX0    = 131.4f;
static constexpr float   s_kPadlockHoleX1    = 132.6f;
static constexpr float   s_kPadlockHoleZ0    = 67.0f;
static constexpr float   s_kPadlockHoleZ1    = 70.9f;
static constexpr float   s_kPadlockShackleY  = -1.95f;
static constexpr float   s_kPadlockBodyY     = -2.00f;
static constexpr float   s_kPadlockHoleY     = -2.05f;
// Slack around the padlock's hit box: the badge is ~10 mm across and the
// tooltip should answer a deliberate hover, not demand marksmanship.
static constexpr float   s_kPadlockHitPadMm  = 2.5f;
static constexpr float   s_kPadlockFill[3]   = { 0.847f, 0.718f, 0.416f };   // warm brass
static constexpr float   s_kPadlockShade[3]  = { 0.478f, 0.376f, 0.149f };   // darker brass
static constexpr float   s_kPadlockHole[3]   = { 0.165f, 0.129f, 0.035f };   // keyhole





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::ColorMatches
//
////////////////////////////////////////////////////////////////////////////////

bool DeskSceneModel::ColorMatches (float r, float g, float b, const float kd[3])
{
    return std::abs (r - kd[0]) <= kKdEpsilon &&
           std::abs (g - kd[1]) <= kKdEpsilon &&
           std::abs (b - kd[2]) <= kKdEpsilon;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::AppendLitTri
//
//  Emits geometry for the SHADER to light: material tint plus the face's
//  normal, both untouched by illumination.
//
//  This used to bake Lambert here on the CPU, and two things about that were
//  wrong in ways no amount of modeling could work around. It took |dot| of
//  the normal, because culling is off and a signed dot could shade a visible
//  back face black -- which made an up-facing wall and a down-facing wall
//  identical, so molded relief had no light-and-shadow flanks and an arrow
//  could not read as an arrow. And it clamped the summed contribution with
//  min(1, sum), so every face past the threshold collapsed onto one value;
//  with two point lights only a few feet away, whether a given face crossed
//  that line depended on where it sat, which is why the same glyph rendered
//  crisply on the bezel's lower band and as mush on its upper one.
//
//  The shader keeps the dot product's sign -- the CAD kernel winds solids
//  outward, so a face the viewer can see already points at them and the
//  guard was never earning its cost -- and rolls the ramp off instead of
//  clipping it. Light positions still arrive in each model's own space --
//  Load() puts them there, and the scene hands them to the renderer before
//  drawing this model.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::AppendLitTri (std::vector<Dxui3DRenderer::Vertex> & out, const ObjTriangle & tri)
{
    float   e1[3]  = { tri.p1[0] - tri.p0[0], tri.p1[1] - tri.p0[1], tri.p1[2] - tri.p0[2] };
    float   e2[3]  = { tri.p2[0] - tri.p0[0], tri.p2[1] - tri.p0[1], tri.p2[2] - tri.p0[2] };
    float   n[3]   = { e1[1] * e2[2] - e1[2] * e2[1],
                       e1[2] * e2[0] - e1[0] * e2[2],
                       e1[0] * e2[1] - e1[1] * e2[0] };
    float   nl     = std::sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);



    for (const float * p : { tri.p0, tri.p1, tri.p2 })
    {
        Dxui3DRenderer::Vertex   v = {};

        v.x = p[0];  v.y = p[1];  v.z = p[2];
        v.r = tri.r;
        v.g = tri.g;
        v.b = tri.b;
        v.a = 1.0f;

        // The face's own normal, carried per vertex. Flat by construction --
        // all three vertices of a triangle get the same one, so a boxy CAD
        // model stays crisp at its edges. Curved features (the fillets on
        // the icons, the funnel's corners) still facet; smoothing those
        // needs vertex welding with an angle threshold, which is a separate
        // job from moving the lighting.
        //
        // A degenerate triangle leaves the normal ZERO, which the shader
        // reads as unlit and passes through at full tint. That matches what
        // the old baked path did with a zero-length normal, which skipped
        // the Lambert loop and kept shade at floor+span == 1.
        if (nl > 0.0f)
        {
            v.nx = n[0] / nl;
            v.ny = n[1] / nl;
            v.nz = n[2] / nl;
        }

        out.push_back (v);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::AppendFlatTri
//
//  Unlit append for the glass and lamp sub-meshes: the glass tint must stay
//  white so the display texture passes through unmodified, and lamp tints are
//  rewritten per frame by the scene's on/off state.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::AppendFlatTri (std::vector<Dxui3DRenderer::Vertex> & out, const ObjTriangle & tri)
{
    for (const float * p : { tri.p0, tri.p1, tri.p2 })
    {
        Dxui3DRenderer::Vertex   v = {};

        v.x = p[0];  v.y = p[1];  v.z = p[2];
        v.r = tri.r;
        v.g = tri.g;
        v.b = tri.b;
        v.a = 1.0f;

        out.push_back (v);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::Load
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneModel::Load (DeskDeviceKind kind, const std::string & objText, const std::string & mtlText)
{
    HRESULT                    hr        = S_OK;
    std::vector<ObjTriangle>   triangles;
    std::vector<std::string>   materialNames;
    std::vector<size_t>        opaqueTris;
    const float              * lampKd    = nullptr;
    float                      anchorLo  = FLT_MAX;
    float                      anchorHi  = -FLT_MAX;
    float                      frontLo   = FLT_MAX;
    float                      frontHi   = -FLT_MAX;
    bool                       lampFound = false;
    bool                       doorOk    = false;



    m_kind = kind;
    m_opaque.clear();
    m_glass.clear();
    m_lamp.clear();
    m_door.clear();
    m_padlock.clear();
    m_lamps.clear();
    m_regions.clear();
    m_surface = {};

    lampKd = IsMonitorKind (kind) ? kMonitorLampKd : kDriveLampKd;

    hr = ObjMeshParser::Parse (objText, mtlText, triangles, materialNames);
    CHRA (hr);

    // Move the room's ceiling lights into this model's own coordinates so
    // the per-face bake needs no transform: the model's x-center rests on
    // the computer's desk-space center line, and a monitor's floor sits on
    // top of the drive stack rather than on the desk.
    {
        float  minX = FLT_MAX;
        float  maxX = -FLT_MAX;
        float  rest = IsMonitorKind (kind) ? s_kMonitorRestMm : 0.0f;

        for (const ObjTriangle & tri : triangles)
        {
            for (const float * p : { tri.p0, tri.p1, tri.p2 })
            {
                minX = (std::min) (minX, p[0]);
                maxX = (std::max) (maxX, p[0]);
            }
        }

        for (size_t i = 0; i < std::size (s_kRoomLights); i++)
        {
            m_lightsModel[i][0] = s_kRoomLights[i][0] + (minX + maxX) * 0.5f;
            m_lightsModel[i][1] = s_kRoomLights[i][1];
            m_lightsModel[i][2] = s_kRoomLights[i][2] - rest;
        }
    }

    for (size_t t = 0; t < triangles.size(); t++)
    {
        const ObjTriangle &  tri  = triangles[t];
        const std::string &  part = ObjMeshParser::MaterialName (tri, materialNames);

        // Metadata first, and it never reaches a vertex buffer: the anchors
        // exist to be measured, not seen. Each names the midpoint of its own
        // extent, so any marker shape at all names the same line or plane.
        if (part == s_kpszBrandAnchor)
        {
            for (const float * p : { tri.p0, tri.p1, tri.p2 })
            {
                anchorLo = (std::min) (anchorLo, p[0]);
                anchorHi = (std::max) (anchorHi, p[0]);
            }

            continue;
        }

        if (part == s_kpszFrontAnchor)
        {
            for (const float * p : { tri.p0, tri.p1, tri.p2 })
            {
                frontLo = (std::min) (frontLo, p[1]);
                frontHi = (std::max) (frontHi, p[1]);
            }

            continue;
        }

        if (IsMonitorKind (kind) && part == s_kpszGlass)
        {
            AppendFlatTri (m_glass, tri);
        }
        else if (part == s_kpszLamp || part == s_kpszLed)
        {
            AppendFlatTri (m_lamp, tri);
        }
        else if (part == s_kpszDoor || part == s_kpszLever || part == s_kpszTab)
        {
            // The door is molded in the same pebbled black as the face around
            // it, and can now SAY so: identity is the part name, so its Kd is
            // free to be the finish it actually has. The flag is still set
            // here because the door is its own batch and never reaches the
            // pebbled branch below.
            size_t  first = m_door.size();

            AppendLitTri (m_door, tri);

            if (kind == DeskDeviceKind::DiskII)
            {
                for (size_t i = first; i < m_door.size(); i++)
                {
                    m_door[i].pebble = 1.0f;
                }
            }
        }
        else if (ColorMatches (tri.r, tri.g, tri.b, kPlatePebbledKd))
        {
            // A finish, not a color. Take the marker off the tint and put it
            // on the pebble flag, so the grain shows up in the LIGHT and the
            // plastic stays the same black as the matte plate beside it.
            size_t  first = m_opaque.size();

            opaqueTris.push_back (t);
            AppendLitTri (m_opaque, tri);

            for (size_t i = first; i < m_opaque.size(); i++)
            {
                m_opaque[i].r      = kPlateMatteRgb[0];
                m_opaque[i].g      = kPlateMatteRgb[1];
                m_opaque[i].b      = kPlateMatteRgb[2];
                m_opaque[i].pebble = 1.0f;
            }
        }
        else
        {
            opaqueTris.push_back (t);
            AppendLitTri (m_opaque, tri);
        }
    }

    // Glass tint is forced white: the picture must pass through unmodified,
    // whatever Kd identified the sheet.
    for (Dxui3DRenderer::Vertex & v : m_glass)
    {
        v.r = v.g = v.b = v.a = 1.0f;
    }

    if (anchorHi >= anchorLo)
    {
        m_brandAxisX = (anchorLo + anchorHi) * 0.5f;
    }

    // No marker means no protruding front: the model's own frontmost point is
    // its frame, which is true of every device that is a plain box.
    m_frontPlaneY = (frontHi >= frontLo) ? (frontLo + frontHi) * 0.5f
                                         : m_boundsMin[1];

    if (IsMonitorKind (kind))
    {
        // The mark sits where each case has room for it: on the //c's chin
        // under the screen, and on the Monitor II's divided right strip, down
        // beside the power button.
        if (kind == DeskDeviceKind::Monitor2)
        {
            // Center the silhouette's MASS, not its grid box: the drawn
            // cassowary is heavier on one side, so box-centering leaves the
            // visual weight off-axis. Sum the set cells' column centers and
            // solve the left edge so the centroid lands on the strip's
            // axis. The proof is in the scene: the strip's calibration
            // ruler shares the stamp's height and perspective, so centroid-
            // on-ruler is judgeable straight off a capture.
            float   cell   = s_kMon2BrandHeightMm / (float) CassoBranding::kGridH;
            double  sumCol = 0.0;
            int     count  = 0;
            int     minCol = CassoBranding::kGridW;
            int     maxCol = -1;

            for (int row = 0; row < CassoBranding::kGridH; row++)
            {
                uint64_t  bits = CassoBranding::SilhouetteRow (row);

                for (int col = 0; col < CassoBranding::kGridW; col++)
                {
                    if ((bits & (1ULL << col)) != 0)
                    {
                        sumCol += (double) col + 0.5;
                        count++;
                        minCol = std::min (minCol, col);
                        maxCol = std::max (maxCol, col);
                    }
                }
            }

            {
                float  centroidCols = (count > 0) ? (float) (sumCol / count)
                                                  : (float) CassoBranding::kGridW * 0.5f;
                float  boxCols      = (maxCol >= minCol) ? (float) (minCol + maxCol + 1) * 0.5f
                                                         : centroidCols;
                float  centerCols   = centroidCols + (boxCols - centroidCols) * s_kMon2BrandBoxCenter;
                float  leftMm       = m_brandAxisX - centerCols * cell;

                BuildBrandStamp (leftMm, s_kMon2BrandTopZMm,
                                 s_kMon2BrandHeightMm, s_kMon2BrandFrontY,
                                 s_kMon2BrandThickMm);
            }
        }
        else
        {
            BuildBrandStamp (s_kBrandLeftMm, s_kBrandTopZMm, s_kBrandHeightMm, s_kBrandFrontY);
        }

        hr = BuildGlassSurface();
        CHRA (hr);

        // Lift the glass a hair toward the viewer: the generated sheet's
        // corners share the cavity front's plane, and equal depth loses the
        // LESS test -- the cavity would clip the corners off the picture.
        // Shifting verts AND the surface together keeps input mapping exact.
        for (Dxui3DRenderer::Vertex & v : m_glass)
        {
            v.y -= kGlassLiftMm;
        }

        m_surface.baseY -= kGlassLiftMm;

        AssignGlassUvs();
    }

    // A device that should carry a lamp but lost it (palette drift in a
    // refined model) is a broken asset, not a runtime condition. Likewise a
    // drive without its door assembly -- the mount/eject animation depends
    // on it.
    lampFound = !m_lamp.empty();
    CBRA (lampFound);

    doorOk = (kind != DeskDeviceKind::DiskII) || !m_door.empty();
    CBRA (doorOk);

    if (kind == DeskDeviceKind::DiskII)
    {
        // NOT derived from the door's own top-back edge any more. That put the
        // pole ON the part, which is a plain hinge, and a plain hinge cannot
        // produce this motion -- see s_kDiskIiDoorPole*. Deriving it also made
        // the mechanism a silent function of the door's bounding box, so
        // remodeling the door moved the mechanism.
        m_doorPivotY = kDiskIiDoorPoleY;
        m_doorPivotZ = kDiskIiDoorPoleZ;

        BuildPadlockStamp();

        // The cassowary (lower-right, the 2D widget's mark) and the IN-USE
        // label pointing at the LED. The DRIVE-number badge text is stamped
        // per-drive by the scene -- it cannot live in the shared model.
        BuildBrandStamp (s_kDriveBrandLeftMm, s_kDriveBrandTopZMm,
                         s_kDriveBrandHeightMm, s_kDriveBrandFrontY);
        StampText (m_opaque, "IN USE >", s_kInUseLeftMm, s_kInUseTopZMm,
                   s_kInUseCellMm, s_kInUseFrontY, s_kInUseRgb);

        BuildWordmarkStamp();
    }

    {
        DeskLampAnchor   anchor;
        float            lo[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
        float            hi[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

        for (const Dxui3DRenderer::Vertex & v : m_lamp)
        {
            lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
            lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
            lo[2] = std::min (lo[2], v.z);  hi[2] = std::max (hi[2], v.z);
        }

        if (!m_lamp.empty())
        {
            anchor.center[0]   = (lo[0] + hi[0]) * 0.5f;
            anchor.center[1]   = (lo[1] + hi[1]) * 0.5f;
            anchor.center[2]   = (lo[2] + hi[2]) * 0.5f;
            anchor.frontY      = lo[1];                     // most proud (viewer at -Y)
            anchor.radiusX     = (hi[0] - lo[0]) * 0.5f;
            anchor.radiusZ     = (hi[2] - lo[2]) * 0.5f;
            anchor.firstVertex = 0;
            anchor.vertexCount = m_lamp.size();
        }

        m_lamps.push_back (anchor);
    }

    AddRegionBoxes();
    ComputeBounds();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildGlassSurface
//
//  Derives the sag sphere from the measured glass mesh rather than assuming
//  the generator's parameters: the rect is the XZ bounding box, the front
//  plane is the highest Y (the corners), the sag is the depth of the lowest Y
//  (the center), and the radius follows from the circle through both --
//  R = (halfDiag^2 + sag^2) / (2 * sag). A refined model that keeps a
//  spherical-sag sheet keeps exact input mapping with no code change.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneModel::BuildGlassSurface()
{
    HRESULT   hr         = S_OK;
    float     lo[3]      = { FLT_MAX, FLT_MAX, FLT_MAX };
    float     hi[3]      = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    float     halfW      = 0.0f;
    float     halfH      = 0.0f;
    float     halfDiag   = 0.0f;
    float     sag        = 0.0f;
    bool      glassFound = !m_glass.empty();
    bool      valid      = false;



    CBRA (glassFound);

    for (const Dxui3DRenderer::Vertex & v : m_glass)
    {
        lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
        lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
        lo[2] = std::min (lo[2], v.z);  hi[2] = std::max (hi[2], v.z);
    }

    halfW    = (hi[0] - lo[0]) * 0.5f;
    halfH    = (hi[2] - lo[2]) * 0.5f;
    halfDiag = std::sqrt (halfW * halfW + halfH * halfH);
    sag      = hi[1] - lo[1];

    // A flat or degenerate sheet cannot carry the display.
    CBRA (halfW > 0.0f && halfH > 0.0f && sag > 0.0f);

    m_surface.x0     = lo[0];
    m_surface.x1     = hi[0];
    m_surface.z0     = lo[2];
    m_surface.z1     = hi[2];
    m_surface.baseY  = hi[1];
    m_surface.radius = (halfDiag * halfDiag + sag * sag) / (2.0f * sag);

    valid = CurvedDisplayMath::IsValid (m_surface);
    CBRA (valid);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::AssignGlassUvs
//
//  Planar XZ projection over the glass rect -- exact for a sheet displaced
//  only along Y (research R4). The same mapping CurvedDisplayMath uses for
//  input, so what the texel shows and what a click hits cannot disagree.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::AssignGlassUvs()
{
    for (Dxui3DRenderer::Vertex & v : m_glass)
    {
        float   pt[3] = { v.x, v.y, v.z };

        CurvedDisplayMath::UvFromModelPoint (m_surface, pt, v.u, v.v);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildBrandStamp
//
//  Stamps the rainbow cassowary as geometry, built from CassoBranding's own
//  silhouette bitmask -- the same mark, same stripes, same concavities as the
//  2D chrome, with no image asset. One quad per contiguous bit run for the
//  face, unlit so the brand colors stay exact. Placement is the caller's: the
//  monitor chin and the drive faceplate both carry it.
//
//  With a thickness the mark also gets SIDE WALLS and becomes a solid standing
//  in the generator's recess rather than a decal lying on the surface. Those
//  walls are lit -- they carry normals, so they shade with the room and are
//  what actually reads as depth; the face stays unlit so the stripes keep
//  their exact values. Walls go on the silhouette BOUNDARY only, found by
//  masking each row against its neighbors, so nothing is emitted inside the
//  mark where it would never be seen. No back face: it would land coplanar
//  with the pocket floor, and the scene draws with culling off.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildBrandStamp (float leftMm, float topZMm, float heightMm, float frontY,
                                      float thicknessMm)
{
    float  rowH     = heightMm / (float) CassoBranding::kGridH;
    float  colW     = rowH;   // the silhouette grid is square-celled
    int    firstRow = CassoBranding::kGridH;
    int    lastRow  = -1;



    for (int row = 0; row < CassoBranding::kGridH; row++)
    {
        if (CassoBranding::SilhouetteRow (row) != 0)
        {
            firstRow = std::min (firstRow, row);
            lastRow  = std::max (lastRow, row);
        }
    }

    if (lastRow < firstRow)
    {
        return;
    }

    if (thicknessMm > 0.0f)
    {
        BuildBrandSolid (leftMm, topZMm, heightMm, frontY, thicknessMm, firstRow, lastRow);
        return;
    }

    for (int row = firstRow; row <= lastRow; row++)
    {
        uint64_t  bits   = CassoBranding::SilhouetteRow (row);
        int       stripe = ((row - firstRow) * CassoBranding::kStripeCount) / (lastRow - firstRow + 1);
        uint32_t  argb   = CassoBranding::StripeColor (stripe);
        float     zTop   = topZMm - (float) row * rowH;
        float     r      = (float) ((argb >> 16) & 0xFF) / 255.0f;
        float     g      = (float) ((argb >> 8) & 0xFF) / 255.0f;
        float     b      = (float) (argb & 0xFF) / 255.0f;
        int       col    = 0;

        while (col < CassoBranding::kGridW)
        {
            int    runStart = 0;
            float  x0       = 0.0f;
            float  x1       = 0.0f;

            if ((bits & (1ULL << col)) == 0)
            {
                col++;
                continue;
            }

            runStart = col;

            while (col < CassoBranding::kGridW && (bits & (1ULL << col)) != 0)
            {
                col++;
            }

            x0 = leftMm + (float) runStart * colW;
            x1 = leftMm + (float) col * colW;

            {
                Dxui3DRenderer::Vertex   quad[6] = {};
                float                    z1      = zTop - rowH;

                quad[0] = { x0, frontY, zTop, 0, 0, r, g, b, 1.0f };
                quad[1] = { x1, frontY, zTop, 0, 0, r, g, b, 1.0f };
                quad[2] = { x1, frontY, z1,   0, 0, r, g, b, 1.0f };
                quad[3] = { x0, frontY, zTop, 0, 0, r, g, b, 1.0f };
                quad[4] = { x1, frontY, z1,   0, 0, r, g, b, 1.0f };
                quad[5] = { x0, frontY, z1,   0, 0, r, g, b, 1.0f };

                m_opaque.insert (m_opaque.end(), quad, quad + 6);
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildBrandSolid
//
//  The cassowary as a solid with a smoothed outline and a rounded top edge.
//
//  The mark's own resolution is the problem this works around. It is a 36x54
//  bitmask, so one cell lands about two screen pixels wide, and both the
//  staircase edges and any round-over are the same order as a cell -- rounding
//  the raw silhouette by the case's own 0.35 mm would erase a one-cell feature
//  outright, taking the beak serrations and the leg with it.
//
//  So the mask is resampled instead of offset. Coverage is measured over a
//  disc a little wider than a cell and thresholded at half, which rounds the
//  staircase off while holding the mark's area and its concavities, and the
//  same field then gives a distance to the outline: cap height ramps from the
//  full face down over the last fraction of a millimeter, so the top edge
//  rolls over rather than breaking square. Erosion never enters it, which is
//  why thin features survive.
//
//  The flat interior stays UNLIT and merged into row runs -- it is most of the
//  mark, and the brand's colors have to come out exact. Only the rolled band
//  and the side walls carry normals and shade, which is the whole of what
//  reads as depth.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildBrandSolid (float leftMm, float topZMm, float heightMm, float frontY,
                                      float thicknessMm, int firstRow, int lastRow)
{
    constexpr int    kSuper   = 4;        // fine cells per silhouette cell
    constexpr float  kSmoothR = 1.15f;    // coverage disc radius, in silhouette cells
    constexpr float  kRollMm  = 0.14f;    // how far down the top edge rolls

    const int    fw     = CassoBranding::kGridW * kSuper;
    const int    fh     = CassoBranding::kGridH * kSuper;
    const int    vw     = fw + 1;
    const float  cell   = heightMm / (float) CassoBranding::kGridH;
    const float  fcell  = cell / (float) kSuper;
    const float  scale  = 2.0f * kSmoothR * cell;   // coverage units -> mm
    const float  backY  = frontY + thicknessMm;
    const int    span   = (int) std::ceil (kSmoothR) + 1;

    std::vector<float>    height ((size_t) vw * (fh + 1), 0.0f);
    std::vector<uint8_t>  inside ((size_t) fw * fh, 0);

    // Vertex pass: coverage -> signed distance to the outline -> cap height.
    for (int vy = 0; vy <= fh; vy++)
    {
        float  oy = (float) vy / (float) kSuper;

        for (int vx = 0; vx <= fw; vx++)
        {
            float  ox   = (float) vx / (float) kSuper;
            int    hits = 0;
            int    seen = 0;

            for (int dy = -span; dy <= span; dy++)
            {
                for (int dx = -span; dx <= span; dx++)
                {
                    int    sy = (int) std::floor (oy) + dy;
                    int    sx = (int) std::floor (ox) + dx;
                    float  ax = (float) sx + 0.5f - ox;
                    float  az = (float) sy + 0.5f - oy;

                    if (ax * ax + az * az > kSmoothR * kSmoothR)
                    {
                        continue;
                    }

                    seen++;

                    if (sy >= 0 && sy < CassoBranding::kGridH &&
                        sx >= 0 && sx < CassoBranding::kGridW &&
                        (CassoBranding::SilhouetteRow (sy) & (1ULL << sx)) != 0)
                    {
                        hits++;
                    }
                }
            }

            {
                float  cov  = (seen > 0) ? (float) hits / (float) seen : 0.0f;
                float  dist = (cov - 0.5f) * scale;
                float  t    = (std::min) (1.0f, (std::max) (0.0f, dist / kRollMm));

                height[(size_t) vy * vw + vx] = frontY + kRollMm * (1.0f - t);
            }
        }
    }

    for (int fy = 0; fy < fh; fy++)
    {
        for (int fx = 0; fx < fw; fx++)
        {
            // A cell belongs to the mark when its own corners sit at or inside
            // the outline -- the height field is already the smoothed shape, so
            // "not fully rolled out" is the test.
            float  h00 = height[(size_t) fy       * vw + fx];
            float  h10 = height[(size_t) fy       * vw + fx + 1];
            float  h01 = height[(size_t) (fy + 1) * vw + fx];
            float  h11 = height[(size_t) (fy + 1) * vw + fx + 1];
            float  avg = (h00 + h10 + h01 + h11) * 0.25f;

            inside[(size_t) fy * fw + fx] = (avg < frontY + kRollMm) ? 1 : 0;
        }
    }

    {
        auto  stripeRgb = [firstRow, lastRow] (int fineRow, float rgb[3])
        {
            int       row    = (std::min) (lastRow, (std::max) (firstRow, fineRow / kSuper));
            int       stripe = ((row - firstRow) * CassoBranding::kStripeCount) / (lastRow - firstRow + 1);
            uint32_t  argb   = CassoBranding::StripeColor (stripe);

            rgb[0] = (float) ((argb >> 16) & 0xFF) / 255.0f;
            rgb[1] = (float) ((argb >> 8) & 0xFF) / 255.0f;
            rgb[2] = (float) (argb & 0xFF) / 255.0f;
        };

        auto  pushTri = [this] (const float a[3], const float b[3], const float c[3],
                                const float n[3], const float rgb[3])
        {
            Dxui3DRenderer::Vertex   tri[3] = {};

            tri[0] = { a[0], a[1], a[2], 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, n[0], n[1], n[2] };
            tri[1] = { b[0], b[1], b[2], 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, n[0], n[1], n[2] };
            tri[2] = { c[0], c[1], c[2], 0, 0, rgb[0], rgb[1], rgb[2], 1.0f, n[0], n[1], n[2] };

            m_opaque.insert (m_opaque.end(), tri, tri + 3);
        };

        auto  pushQuad = [&pushTri] (const float p00[3], const float p10[3],
                                     const float p11[3], const float p01[3],
                                     const float n[3], const float rgb[3])
        {
            pushTri (p00, p10, p11, n, rgb);
            pushTri (p00, p11, p01, n, rgb);
        };

        const float  kFlatN[3] = { 0.0f, 0.0f, 0.0f };   // zero normal == unlit

        for (int fy = 0; fy < fh; fy++)
        {
            float  rgb[3] = {};
            float  zTop   = topZMm - (float) fy * fcell;
            float  zBot   = zTop - fcell;
            int    fx     = 0;

            stripeRgb (fy, rgb);

            // Flat interior, merged into runs: unlit, so the stripes are exact.
            while (fx < fw)
            {
                int  runStart = fx;

                while (fx < fw && inside[(size_t) fy * fw + fx] != 0 &&
                       height[(size_t) fy       * vw + fx]     <= frontY + 1e-4f &&
                       height[(size_t) fy       * vw + fx + 1] <= frontY + 1e-4f &&
                       height[(size_t) (fy + 1) * vw + fx]     <= frontY + 1e-4f &&
                       height[(size_t) (fy + 1) * vw + fx + 1] <= frontY + 1e-4f)
                {
                    fx++;
                }

                if (fx > runStart)
                {
                    float  x0     = leftMm + (float) runStart * fcell;
                    float  x1     = leftMm + (float) fx * fcell;
                    float  p00[3] = { x0, frontY, zTop };
                    float  p10[3] = { x1, frontY, zTop };
                    float  p11[3] = { x1, frontY, zBot };
                    float  p01[3] = { x0, frontY, zBot };

                    pushQuad (p00, p10, p11, p01, kFlatN, rgb);
                    continue;
                }

                fx++;
            }

            // The rolled band and the walls, cell by cell.
            for (fx = 0; fx < fw; fx++)
            {
                float  h00 = height[(size_t) fy       * vw + fx];
                float  h10 = height[(size_t) fy       * vw + fx + 1];
                float  h01 = height[(size_t) (fy + 1) * vw + fx];
                float  h11 = height[(size_t) (fy + 1) * vw + fx + 1];
                float  x0  = leftMm + (float) fx * fcell;
                float  x1  = x0 + fcell;

                if (inside[(size_t) fy * fw + fx] == 0)
                {
                    continue;
                }

                if (h00 > frontY + 1e-4f || h10 > frontY + 1e-4f ||
                    h01 > frontY + 1e-4f || h11 > frontY + 1e-4f)
                {
                    float  p00[3] = { x0, h00, zTop };
                    float  p10[3] = { x1, h10, zTop };
                    float  p11[3] = { x1, h11, zBot };
                    float  p01[3] = { x0, h01, zBot };
                    float  n[3]   = { (h10 - h00) * fcell, -fcell * fcell, -(h01 - h00) * fcell };
                    float  len    = std::sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);

                    if (len > 1e-6f)
                    {
                        n[0] /= len;
                        n[1] /= len;
                        n[2] /= len;
                        pushQuad (p00, p10, p11, p01, n, rgb);
                    }
                }

                // Side walls, wherever the neighbor is not part of the mark.
                if (fx == 0 || inside[(size_t) fy * fw + fx - 1] == 0)
                {
                    float  a[3] = { x0, h00,   zTop };
                    float  b[3] = { x0, h01,   zBot };
                    float  c[3] = { x0, backY, zBot };
                    float  d[3] = { x0, backY, zTop };
                    float  n[3] = { -1.0f, 0.0f, 0.0f };

                    pushQuad (a, b, c, d, n, rgb);
                }

                if (fx == fw - 1 || inside[(size_t) fy * fw + fx + 1] == 0)
                {
                    float  a[3] = { x1, h10,   zTop };
                    float  b[3] = { x1, h11,   zBot };
                    float  c[3] = { x1, backY, zBot };
                    float  d[3] = { x1, backY, zTop };
                    float  n[3] = { 1.0f, 0.0f, 0.0f };

                    pushQuad (a, b, c, d, n, rgb);
                }

                if (fy == 0 || inside[(size_t) (fy - 1) * fw + fx] == 0)
                {
                    float  a[3] = { x0, h00,   zTop };
                    float  b[3] = { x1, h10,   zTop };
                    float  c[3] = { x1, backY, zTop };
                    float  d[3] = { x0, backY, zTop };
                    float  n[3] = { 0.0f, 0.0f, 1.0f };

                    pushQuad (a, b, c, d, n, rgb);
                }

                if (fy == fh - 1 || inside[(size_t) (fy + 1) * fw + fx] == 0)
                {
                    float  a[3] = { x0, h01,   zBot };
                    float  b[3] = { x1, h11,   zBot };
                    float  c[3] = { x1, backY, zBot };
                    float  d[3] = { x0, backY, zBot };
                    float  n[3] = { 0.0f, 0.0f, -1.0f };

                    pushQuad (a, b, c, d, n, rgb);
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildPadlockStamp
//
//  The write-protect cue: a small brass padlock on the drive faceplate,
//  right of the IN-USE LED -- the 2D widget's badge carried into the model.
//  Flat unlit quads (brand-stamp style) so the brass reads exactly; the
//  scene draws them only while the mounted disk is protected.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildPadlockStamp()
{
    auto pushQuad = [this] (float x0, float z0, float x1, float z1, float y, const float rgb[3])
    {
        Dxui3DRenderer::Vertex   quad[6] = {};

        quad[0] = { x0, y, z1, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[1] = { x1, y, z1, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[2] = { x1, y, z0, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[3] = { x0, y, z1, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[4] = { x1, y, z0, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
        quad[5] = { x0, y, z0, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };

        m_padlock.insert (m_padlock.end(), quad, quad + 6);
    };

    float   legX0 = s_kPadlockBodyX0 + s_kPadlockArchInset;
    float   legX1 = s_kPadlockBodyX1 - s_kPadlockArchInset;



    // Shackle: two legs rising from the body, bridged by the arch bar.
    pushQuad (legX0, s_kPadlockBodyZ1, legX0 + s_kPadlockLegW, s_kPadlockArchZ1, s_kPadlockShackleY, s_kPadlockShade);
    pushQuad (legX1 - s_kPadlockLegW, s_kPadlockBodyZ1, legX1, s_kPadlockArchZ1, s_kPadlockShackleY, s_kPadlockShade);
    pushQuad (legX0, s_kPadlockArchZ1 - s_kPadlockLegW, legX1, s_kPadlockArchZ1, s_kPadlockShackleY, s_kPadlockShade);

    // Body, then the keyhole floating on it.
    pushQuad (s_kPadlockBodyX0, s_kPadlockBodyZ0, s_kPadlockBodyX1, s_kPadlockBodyZ1, s_kPadlockBodyY, s_kPadlockFill);
    pushQuad (s_kPadlockHoleX0, s_kPadlockHoleZ0, s_kPadlockHoleX1, s_kPadlockHoleZ1, s_kPadlockHoleY, s_kPadlockHole);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::RotateDoorVerts
//
//  Rigid rotation about the hinge -- the X-axis line at (pivotY, pivotZ).
//  The signs put a point below the hinge out toward the viewer (-Y) as the
//  angle grows: at 90 degrees the door lies flat, pointing at the camera.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::RotateDoorVerts (const std::vector<Dxui3DRenderer::Vertex> & base,
                                      float                                       pivotY,
                                      float                                       pivotZ,
                                      float                                       angleRad,
                                      std::vector<Dxui3DRenderer::Vertex>       & out)
{
    float   c = std::cos (angleRad);
    float   s = std::sin (angleRad);



    out = base;

    for (Dxui3DRenderer::Vertex & v : out)
    {
        float   dy = v.y - pivotY;
        float   dz = v.z - pivotZ;

        v.y = pivotY + dy * c + dz * s;
        v.z = pivotZ - dy * s + dz * c;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::StampText
//
//  Each glyph row is 5 bits, bit 0 the LEFT column; unknown characters stamp
//  as spaces. Runs merge within a row exactly like the brand stamp.
//
////////////////////////////////////////////////////////////////////////////////

// The label font: only what the scene's drive labels use. '>' is the
// LED-pointer triangle.
//
// SEVEN BY NINE, and written as PICTURES rather than hex. The old font was
// 5x7 packed into uint8_t rows, and both halves of that were a problem: five
// columns cannot hold a letterform with any weight to it, so the labels read
// as chunky approximations, and a wall of hex is not something anyone can
// edit -- the shape is invisible until it renders.
//
// A '#' is on and every other character is off, so the glyph in the source
// looks like the glyph on the drive. That matters more than the parse cost,
// which is paid once at model load and never again.
struct SceneGlyph
{
    char          ch;
    const char *  rows[9];
};

static constexpr int  s_kGlyphW    = 7;
static constexpr int  s_kGlyphH    = 9;
static constexpr int  s_kGlyphAdv  = 8;   // 7 columns + 1 of tracking

static constexpr SceneGlyph  s_kSceneFont[] =
{
    { 'D', { "######.",
             "##...##",
             "##....#",
             "##....#",
             "##....#",
             "##....#",
             "##....#",
             "##...##",
             "######." } },

    { 'R', { "######.",
             "##...##",
             "##...##",
             "##...##",
             "######.",
             "##.##..",
             "##..##.",
             "##...##",
             "##....#" } },

    { 'I', { "#######",
             "...#...",
             "...#...",
             "...#...",
             "...#...",
             "...#...",
             "...#...",
             "...#...",
             "#######" } },

    { 'V', { "##...##",
             "##...##",
             "##...##",
             "##...##",
             "##...##",
             ".##.##.",
             ".##.##.",
             "..###..",
             "...#..." } },

    { 'E', { "#######",
             "##.....",
             "##.....",
             "##.....",
             "######.",
             "##.....",
             "##.....",
             "##.....",
             "#######" } },

    { 'N', { "##....#",
             "###...#",
             "####..#",
             "##.#..#",
             "##..#.#",
             "##...##",
             "##....#",
             "##....#",
             "##....#" } },

    { 'U', { "##...##",
             "##...##",
             "##...##",
             "##...##",
             "##...##",
             "##...##",
             "##...##",
             "##...##",
             ".#####." } },

    { 'S', { ".#####.",
             "##...##",
             "##.....",
             "###....",
             ".####..",
             "....###",
             ".....##",
             "##...##",
             ".#####." } },

    { '1', { "...##..",
             "..###..",
             ".####..",
             "...##..",
             "...##..",
             "...##..",
             "...##..",
             "...##..",
             ".######" } },

    { '2', { ".#####.",
             "##...##",
             ".....##",
             "....##.",
             "...##..",
             "..##...",
             ".##....",
             "##.....",
             "#######" } },

    { '>', { "##.....",
             "###....",
             "####...",
             "#####..",
             "######.",
             "#####..",
             "####...",
             "###....",
             "##....." } },
};


// The "disk ][" logotype as it is set on the drive's lid: a SILHOUETTE, not
// type. The mark is a logotype rather than a string -- its lowercase d has a
// slab that no font of ours has, and the ][ is a pair of drawn bars, not two
// bracket characters -- so it is stored as the shape it is, the same way the
// cassowary is.
//
// Rows read front-to-back on the lid: row 0 is the mark's top, which is the
// edge AWAY from the viewer, so the label reads right way up to someone
// standing at the drive.
static const char * const  s_kDiskWordmark[] =
{
    "...............................................................######.#######",
    ".............######.######...............######..............########.#######",
    ".............######.######...............######..............########.#######",
    ".............######.######...............######..............########.#######",
    ".............######..#####...............######...............#######.#######",
    ".............######..##.##.....########..######....#####.......######.#####..",
    ".....#######.######..#####...###########.######...#######......######.#####..",
    "....########.######.######..############.######...#######......######.#####..",
    "...#########.######.######.#############.######...#######......######.#####..",
    "..##########.######.######.#############.######..#######.......######.#####..",
    ".###########.######.######.#############.###############.......######.#####..",
    ".########....######.######.#####.........##############........######.#####..",
    "#######......######.######.###########...#############.........######.#####..",
    "######.......######.######.############..#############.........######.#####..",
    "######.......######.######..############.##############........######.#####..",
    "######.......######.######..############.###############.......######.#####..",
    "#######......######..#####....##########.######...#######......######.#####..",
    "########.....######.######..#.......####.######...#######......######.#####..",
    ".##################.######.#############.######...#######....########.#######",
    ".##################.######.#############.######...#######....########.#######",
    "..#################.######.#############.######...#######....########.#######",
    "...################.######.############..######....######....########.#######",
    ".....##############..#####.##########.....#####....#####.....................",
};

static constexpr int  s_kWordmarkRows = (int) (sizeof (s_kDiskWordmark) / sizeof (s_kDiskWordmark[0]));
static constexpr int  s_kWordmarkCols = 77;





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BuildWordmarkStamp
//
//  The "disk ][" logotype, printed low on the faceplate below the IN-USE
//  legend and its lamp -- opposite the cassowary, which holds the other
//  corner.
//
//  A SILHOUETTE, not type: the mark's lowercase d carries a slab no font of
//  ours has and its ][ is a pair of drawn bars rather than two bracket
//  characters, so it is stored as the shape it is.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildWordmarkStamp()
{
    std::vector<float>  ink ((size_t) s_kWordmarkRows * 3);

    for (int row = 0; row < s_kWordmarkRows; row++)
    {
        ink[(size_t) row * 3 + 0] = s_kWordRgb[0];
        ink[(size_t) row * 3 + 1] = s_kWordRgb[1];
        ink[(size_t) row * 3 + 2] = s_kWordRgb[2];
    }

    StampFaceMask (m_opaque, s_kDiskWordmark, s_kWordmarkRows, s_kWordmarkCols,
                   s_kWordLeftMm, s_kWordTopZMm, s_kWordCellMm, s_kWordFrontY,
                   ink.data());
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::StampFaceMask
//
//  A silhouette laid on a front face, one merged quad per horizontal run.
//  Unlit, like the other stamps: printed marks keep their exact ink values
//  rather than taking the room's light, and the surface they sit on carries
//  all the shading the eye needs to place them.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::StampFaceMask (std::vector<Dxui3DRenderer::Vertex> & out,
                                    const char * const                  * rows,
                                    int                                   rowCount,
                                    int                                   colCount,
                                    float                                 leftMm,
                                    float                                 topZMm,
                                    float                                 cellMm,
                                    float                                 frontY,
                                    const float                         * rowRgb)
{
    for (int row = 0; row < rowCount; row++)
    {
        const char *  bits = rows[row];
        float         zTop = topZMm - (float) row * cellMm;
        float         z1   = zTop - cellMm;
        float         r    = rowRgb[row * 3 + 0];
        float         g    = rowRgb[row * 3 + 1];
        float         b    = rowRgb[row * 3 + 2];
        int           col  = 0;

        while (col < colCount)
        {
            int    runStart = 0;
            float  x0       = 0.0f;
            float  x1       = 0.0f;

            if (bits[col] != '#')
            {
                col++;
                continue;
            }

            runStart = col;

            while (col < colCount && bits[col] == '#')
            {
                col++;
            }

            x0 = leftMm + (float) runStart * cellMm;
            x1 = leftMm + (float) col * cellMm;

            {
                Dxui3DRenderer::Vertex   quad[6] = {};

                quad[0] = { x0, frontY, zTop, 0, 0, r, g, b, 1.0f };
                quad[1] = { x1, frontY, zTop, 0, 0, r, g, b, 1.0f };
                quad[2] = { x1, frontY, z1,   0, 0, r, g, b, 1.0f };
                quad[3] = { x0, frontY, zTop, 0, 0, r, g, b, 1.0f };
                quad[4] = { x1, frontY, z1,   0, 0, r, g, b, 1.0f };
                quad[5] = { x0, frontY, z1,   0, 0, r, g, b, 1.0f };

                out.insert (out.end(), quad, quad + 6);
            }
        }
    }
}





void DeskSceneModel::StampText (std::vector<Dxui3DRenderer::Vertex> & out,
                                const char                          * text,
                                float                                 leftMm,
                                float                                 topZMm,
                                float                                 cellMm,
                                float                                 frontY,
                                const float                           rgb[3])
{
    float  penX = leftMm;



    for (const char * pCh = text; *pCh != '\0'; pCh++)
    {
        const SceneGlyph *  glyph = nullptr;

        for (const SceneGlyph & candidate : s_kSceneFont)
        {
            if (candidate.ch == *pCh)
            {
                glyph = &candidate;
                break;
            }
        }

        if (glyph != nullptr)
        {
            for (int row = 0; row < s_kGlyphH; row++)
            {
                const char *  bits = glyph->rows[row];
                float         zTop = topZMm - (float) row * cellMm;
                int           col  = 0;

                while (col < s_kGlyphW)
                {
                    int  runStart = 0;

                    if (bits[col] != '#')
                    {
                        col++;
                        continue;
                    }

                    runStart = col;

                    while (col < s_kGlyphW && bits[col] == '#')
                    {
                        col++;
                    }

                    {
                        Dxui3DRenderer::Vertex   quad[6] = {};
                        float                    x0      = penX + (float) runStart * cellMm;
                        float                    x1      = penX + (float) col * cellMm;
                        float                    z1      = zTop - cellMm;

                        quad[0] = { x0, frontY, zTop, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
                        quad[1] = { x1, frontY, zTop, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
                        quad[2] = { x1, frontY, z1,   0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
                        quad[3] = { x0, frontY, zTop, 0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
                        quad[4] = { x1, frontY, z1,   0, 0, rgb[0], rgb[1], rgb[2], 1.0f };
                        quad[5] = { x0, frontY, z1,   0, 0, rgb[0], rgb[1], rgb[2], 1.0f };

                        out.insert (out.end(), quad, quad + 6);
                    }
                }
            }
        }

        penX += (float) s_kGlyphAdv * cellMm;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::AddRegionBoxes
//
//  Declaration order is precedence: the eject region sits inside the body
//  box and must list first, mirroring DriveWidget::HitTest checking the
//  eject rect before the body rect.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::AddRegionBoxes()
{
    DeskRegionBox   box;



    if (m_kind != DeskDeviceKind::DiskII)
    {
        return;
    }

    memcpy (box.boxMin, s_kDiskIiEjectMin, sizeof (box.boxMin));
    memcpy (box.boxMax, s_kDiskIiEjectMax, sizeof (box.boxMax));
    box.region = DriveWidgetRegion::Eject;
    m_regions.push_back (box);

    // The padlock ranks BELOW eject and above body. Its box overlaps the top
    // of the eject zone, and declaration order is precedence -- listing it
    // first would have quietly taken clicks away from eject in that strip for
    // the sake of a tooltip. The badge keeps everything above the slot, which
    // is most of it. Slack all round so the badge is not a pixel hunt, and
    // the near face reaches in front of the plate it stands on.
    box.boxMin[0] = s_kPadlockBodyX0 - s_kPadlockHitPadMm;
    box.boxMin[1] = s_kPadlockHoleY - 1.0f;
    box.boxMin[2] = s_kPadlockBodyZ0 - s_kPadlockHitPadMm;
    box.boxMax[0] = s_kPadlockBodyX1 + s_kPadlockHitPadMm;
    box.boxMax[1] = 0.5f;
    box.boxMax[2] = s_kPadlockArchZ1 + s_kPadlockHitPadMm;
    box.region    = DriveWidgetRegion::Padlock;
    m_regions.push_back (box);

    memcpy (box.boxMin, s_kDiskIiBodyMin, sizeof (box.boxMin));
    memcpy (box.boxMax, s_kDiskIiBodyMax, sizeof (box.boxMax));
    box.region = DriveWidgetRegion::Body;
    m_regions.push_back (box);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::ComputeBounds
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::ComputeBounds()
{
    float   lo[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
    float   hi[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };



    for (const std::vector<Dxui3DRenderer::Vertex> * batch : { &m_opaque, &m_glass, &m_lamp, &m_door })
    {
        for (const Dxui3DRenderer::Vertex & v : *batch)
        {
            lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
            lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
            lo[2] = std::min (lo[2], v.z);  hi[2] = std::max (hi[2], v.z);
        }
    }

    memcpy (m_boundsMin, lo, sizeof (m_boundsMin));
    memcpy (m_boundsMax, hi, sizeof (m_boundsMax));

    ComputeGroundFootprint();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::ComputeGroundFootprint
//
//  The XY extent of the geometry that actually TOUCHES the ground, not the
//  full bounding box: the monitor's bezel overhangs its shell by 20 mm and
//  the drives' lips overhang theirs, so a shadow sized to the box would jut
//  out in front of the object it belongs to. Anything within kGroundBandMm of
//  the model's lowest vertex counts as contact.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::ComputeGroundFootprint()
{
    float   height  = m_boundsMax[2] - m_boundsMin[2];
    float   band    = std::max (kGroundBandMm, height * kGroundBandFraction);
    float   ceiling = m_boundsMin[2] + band;
    float   lo[2]   = { FLT_MAX, FLT_MAX };
    float   hi[2]   = { -FLT_MAX, -FLT_MAX };



    for (const std::vector<Dxui3DRenderer::Vertex> * batch : { &m_opaque, &m_glass, &m_lamp, &m_door })
    {
        for (const Dxui3DRenderer::Vertex & v : *batch)
        {
            if (v.z <= ceiling)
            {
                lo[0] = std::min (lo[0], v.x);  hi[0] = std::max (hi[0], v.x);
                lo[1] = std::min (lo[1], v.y);  hi[1] = std::max (hi[1], v.y);
            }
        }
    }

    // No contact geometry, or a patch with no area on one axis (a case that
    // rests on a single edge), collapses to the box -- the honest fallback,
    // and the one that keeps a caller from having to special-case a
    // zero-width footprint.
    if (lo[0] >= hi[0] || lo[1] >= hi[1])
    {
        lo[0] = m_boundsMin[0];  hi[0] = m_boundsMax[0];
        lo[1] = m_boundsMin[1];  hi[1] = m_boundsMax[1];
    }

    m_footprintMin[0] = lo[0];  m_footprintMax[0] = hi[0];
    m_footprintMin[1] = lo[1];  m_footprintMax[1] = hi[1];
}
