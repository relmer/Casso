#include "Pch.h"

#include "Ui/Scene/DeskSceneModel.h"

#include "Devices/Printer/ObjMeshParser.h"
#include "Ui/Chrome/DriveWidget.h"





// Baked light direction in model space: the printer scene's light
// (0.35, -0.7, 0.61) in Y-up render space, expressed in the models' Z-up
// space (X right, Y back, Z up), normalized on use.
static constexpr float   s_kLightDir[3] = { 0.35f, -0.61f, -0.70f };

// The printer scene's two-sided Lambert ramp: fully shadowed faces keep 30%
// of their color so nothing reads as a black hole.
static constexpr float   s_kShadeFloor  = 0.30f;
static constexpr float   s_kShadeSpan   = 0.70f;

// DiskII interactive regions, model space (mm). The eject region wraps the
// slot + door bar + latch; the body box wraps the whole case including the
// proud front furniture.
static constexpr float   s_kDiskIiEjectMin[3] = {  14.0f, -5.0f, 44.0f };
static constexpr float   s_kDiskIiEjectMax[3] = { 141.0f,  3.0f, 63.0f };
static constexpr float   s_kDiskIiBodyMin[3]  = {   0.0f, -5.0f,  0.0f };
static constexpr float   s_kDiskIiBodyMax[3]  = { 155.0f, 222.0f, 86.0f };





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



    m_kind = kind;
    m_opaque.clear();
    m_glass.clear();
    m_lamp.clear();
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
        hr = BuildGlassSurface();
        CHRA (hr);

        AssignGlassUvs();
    }

    // A device that should carry a lamp but lost it (palette drift in a
    // refined model) is a broken asset, not a runtime condition.
    lampFound = !m_lamp.empty();
    CBRA (lampFound);

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

        anchor.center[0]   = (lo[0] + hi[0]) * 0.5f;
        anchor.center[1]   = (lo[1] + hi[1]) * 0.5f;
        anchor.center[2]   = (lo[2] + hi[2]) * 0.5f;
        anchor.firstVertex = 0;
        anchor.vertexCount = m_lamp.size();

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



    for (const std::vector<Dxui3DRenderer::Vertex> * batch : { &m_opaque, &m_glass, &m_lamp })
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
}
