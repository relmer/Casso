#include "Pch.h"

#include "Ui/Scene/DeskSceneModel.h"

#include "Devices/Printer/ObjMeshParser.h"
#include "Ui/Chrome/CassoBranding.h"
#include "Ui/Chrome/DriveWidget.h"





// Baked light direction in model space (X right, Y back, Z up): overhead
// front-left, like the reference photos -- raking enough that the beveled
// front surfaces separate from each other, which is what sells the 3D read.
static constexpr float   s_kLightDir[3] = { -0.35f, -0.55f, 0.75f };

// Two-sided Lambert ramp. The floor is deliberately low: a shallow ramp
// reads as a flat 2D cutout.
static constexpr float   s_kShadeFloor  = 0.16f;
static constexpr float   s_kShadeSpan   = 0.84f;

// Brand stamp placement on the monitor chin (model mm): the cassowary spans
// this box, proud of the bezel plate's front face (y = -10), inside the
// slimmed chin band (bezel z 9 .. 29).
static constexpr float   s_kBrandLeftMm   = 24.0f;
static constexpr float   s_kBrandTopZMm   = 27.0f;
static constexpr float   s_kBrandHeightMm = 16.0f;
static constexpr float   s_kBrandFrontY   = -10.6f;

// The drive's cassowary, lower-right of the faceplate like the 2D widget,
// proud of the black plate (front y = -1).
static constexpr float   s_kDriveBrandLeftMm   = 127.0f;
static constexpr float   s_kDriveBrandTopZMm   = 34.0f;
static constexpr float   s_kDriveBrandHeightMm = 26.0f;
static constexpr float   s_kDriveBrandFrontY   = -1.8f;

// The IN-USE label: "IN USE" plus the pointer triangle, sitting to the
// LED's left at the LED's height (the 2D widget's arrangement).
static constexpr float   s_kInUseLeftMm  = 20.0f;
static constexpr float   s_kInUseTopZMm  = 19.5f;
static constexpr float   s_kInUseCellMm  = 1.0f;
static constexpr float   s_kInUseFrontY  = -1.8f;
static constexpr float   s_kInUseRgb[3]  = { 0.750f, 0.730f, 0.700f };

// DiskII interactive regions, model space (mm). The eject region wraps the
// slot + door bar + latch; the body box wraps the whole case including the
// proud front furniture.
static constexpr float   s_kDiskIiEjectMin[3] = {  14.0f, -5.0f, 44.0f };
static constexpr float   s_kDiskIiEjectMax[3] = { 141.0f,  3.0f, 63.0f };
static constexpr float   s_kDiskIiBodyMin[3]  = {   0.0f, -5.0f,  0.0f };
static constexpr float   s_kDiskIiBodyMax[3]  = { 155.0f, 222.0f, 86.0f };

// Write-protect padlock stamp on the drive faceplate (model mm): brass body
// with a shackle arch and a keyhole, top-right beside the badge row like
// the 2D widget's badge. Flat proud quads like the brand stamp; each layer
// floats a hair nearer the viewer than the one it sits on so depth never
// ties.
static constexpr float   s_kPadlockBodyX0    = 127.0f;
static constexpr float   s_kPadlockBodyX1    = 137.0f;
static constexpr float   s_kPadlockBodyZ0    = 58.0f;
static constexpr float   s_kPadlockBodyZ1    = 66.5f;
static constexpr float   s_kPadlockArchZ1    = 72.0f;
static constexpr float   s_kPadlockLegW      = 1.7f;
static constexpr float   s_kPadlockArchInset = 1.5f;
static constexpr float   s_kPadlockHoleX0    = 131.4f;
static constexpr float   s_kPadlockHoleX1    = 132.6f;
static constexpr float   s_kPadlockHoleZ0    = 60.0f;
static constexpr float   s_kPadlockHoleZ1    = 63.5f;
static constexpr float   s_kPadlockShackleY  = -1.95f;
static constexpr float   s_kPadlockBodyY     = -2.00f;
static constexpr float   s_kPadlockHoleY     = -2.05f;
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
//  Per-face Lambert baked on the CPU, two-sided (|dot|) since the meshes are
//  drawn with culling off -- the same lighting model the printer scene bakes.
//  The shade premultiplies into the vertex tint; the pixel shader is tex*col
//  and the untextured path samples opaque white, so tint IS the lit color.
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
    float   ll     = std::sqrt (s_kLightDir[0] * s_kLightDir[0] +
                                s_kLightDir[1] * s_kLightDir[1] +
                                s_kLightDir[2] * s_kLightDir[2]);
    float   d      = 0.0f;
    float   shade  = s_kShadeFloor + s_kShadeSpan;



    if (nl > 0.0f)
    {
        d     = (n[0] * s_kLightDir[0] + n[1] * s_kLightDir[1] + n[2] * s_kLightDir[2]) / (nl * ll);
        shade = s_kShadeFloor + s_kShadeSpan * std::abs (d);
    }

    for (const float * p : { tri.p0, tri.p1, tri.p2 })
    {
        Dxui3DRenderer::Vertex   v = {};

        v.x = p[0];  v.y = p[1];  v.z = p[2];
        v.r = tri.r * shade;
        v.g = tri.g * shade;
        v.b = tri.b * shade;
        v.a = 1.0f;

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
    const float              * lampKd    = nullptr;
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

    lampKd = (kind == DeskDeviceKind::Monitor2c) ? kMonitorLampKd : kDriveLampKd;

    hr = ObjMeshParser::Parse (objText, mtlText, triangles);
    CHRA (hr);

    for (const ObjTriangle & tri : triangles)
    {
        if (kind == DeskDeviceKind::Monitor2c && ColorMatches (tri.r, tri.g, tri.b, kGlassKd))
        {
            AppendFlatTri (m_glass, tri);
        }
        else if (ColorMatches (tri.r, tri.g, tri.b, lampKd))
        {
            AppendFlatTri (m_lamp, tri);
        }
        else if (kind == DeskDeviceKind::DiskII &&
                 (ColorMatches (tri.r, tri.g, tri.b, kDriveDoorKd) ||
                  ColorMatches (tri.r, tri.g, tri.b, kDriveLatchKd)))
        {
            AppendLitTri (m_door, tri);
        }
        else
        {
            AppendLitTri (m_opaque, tri);
        }
    }

    // Glass tint is forced white: the picture must pass through unmodified,
    // whatever Kd identified the sheet.
    for (Dxui3DRenderer::Vertex & v : m_glass)
    {
        v.r = v.g = v.b = v.a = 1.0f;
    }

    if (kind == DeskDeviceKind::Monitor2c)
    {
        BuildBrandStamp (s_kBrandLeftMm, s_kBrandTopZMm, s_kBrandHeightMm, s_kBrandFrontY);

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
        float   hiY = -FLT_MAX;
        float   hiZ = -FLT_MAX;

        for (const Dxui3DRenderer::Vertex & v : m_door)
        {
            hiY = std::max (hiY, v.y);
            hiZ = std::max (hiZ, v.z);
        }

        m_doorPivotY = hiY;
        m_doorPivotZ = hiZ;

        BuildPadlockStamp();

        // The cassowary (lower-right, the 2D widget's mark) and the IN-USE
        // label pointing at the LED. The DRIVE-number badge text is stamped
        // per-drive by the scene -- it cannot live in the shared model.
        BuildBrandStamp (s_kDriveBrandLeftMm, s_kDriveBrandTopZMm,
                         s_kDriveBrandHeightMm, s_kDriveBrandFrontY);
        StampText (m_opaque, "IN USE >", s_kInUseLeftMm, s_kInUseTopZMm,
                   s_kInUseCellMm, s_kInUseFrontY, s_kInUseRgb);
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
            anchor.frontY      = lo[1];                                    // most proud (viewer at -Y)
            anchor.radius      = std::max (hi[0] - lo[0], hi[2] - lo[2]) * 0.5f;
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
//  Stamps the rainbow cassowary as flat geometry, built from CassoBranding's
//  own silhouette bitmask -- the same mark, same stripes, same concavities
//  as the 2D chrome, with no image asset. One proud quad per contiguous bit
//  run, unlit so the brand colors stay exact. Placement is the caller's:
//  the monitor chin and the drive faceplate both carry it.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BuildBrandStamp (float leftMm, float topZMm, float heightMm, float frontY)
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
struct SceneGlyph
{
    char     ch;
    uint8_t  rows[7];
};

static constexpr SceneGlyph  s_kSceneFont[] =
{
    { 'D', { 0x0F, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0F } },
    { 'R', { 0x0F, 0x11, 0x11, 0x0F, 0x05, 0x09, 0x11 } },
    { 'I', { 0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F } },
    { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04 } },
    { 'E', { 0x1F, 0x01, 0x01, 0x0F, 0x01, 0x01, 0x1F } },
    { 'N', { 0x11, 0x13, 0x15, 0x19, 0x11, 0x11, 0x11 } },
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E } },
    { 'S', { 0x1E, 0x01, 0x01, 0x0E, 0x10, 0x10, 0x0F } },
    { '1', { 0x04, 0x06, 0x04, 0x04, 0x04, 0x04, 0x0E } },
    { '2', { 0x0E, 0x11, 0x10, 0x0C, 0x02, 0x01, 0x1F } },
    { '>', { 0x01, 0x03, 0x07, 0x0F, 0x07, 0x03, 0x01 } },
};

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
            for (int row = 0; row < 7; row++)
            {
                uint8_t  bits = glyph->rows[row];
                float    zTop = topZMm - (float) row * cellMm;
                int      col  = 0;

                while (col < 5)
                {
                    int  runStart = 0;

                    if ((bits & (1 << col)) == 0)
                    {
                        col++;
                        continue;
                    }

                    runStart = col;

                    while (col < 5 && (bits & (1 << col)) != 0)
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

        penX += 6.0f * cellMm;   // 5 columns + 1 of tracking
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
    float   ceiling = m_boundsMin[2] + kGroundBandMm;
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

    // No contact geometry at all (a model floating above its own origin)
    // collapses to the box, which is the honest fallback.
    if (lo[0] > hi[0])
    {
        lo[0] = m_boundsMin[0];  hi[0] = m_boundsMax[0];
        lo[1] = m_boundsMin[1];  hi[1] = m_boundsMax[1];
    }

    m_footprintMin[0] = lo[0];  m_footprintMax[0] = hi[0];
    m_footprintMin[1] = lo[1];  m_footprintMax[1] = hi[1];
}
