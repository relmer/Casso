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

// Inverse-square falloff, normalized so a face at ceiling-height distance
// under a light bakes at full span. The near-left light dominates and the
// far-right one back-fills, which is what two real fixtures would do.
static constexpr float   s_kLightRefMm  = 1524.0f;

// Where the monitor's model-space floor rests in desk space: on top of the
// drive stack. A nominal Disk II height serves both pairings -- at five
// feet of throw the shorter //c drives move the light angle by well under
// a degree.
static constexpr float   s_kMonitorRestMm = 96.0f;

// Two-sided Lambert ramp. The floor is deliberately low: a shallow ramp
// reads as a flat 2D cutout.
static constexpr float   s_kShadeFloor  = 0.16f;
static constexpr float   s_kShadeSpan   = 0.84f;

// The lamp as a REAL emitter, baked at load into a second copy of the body.
// A glow disc drawn over the lens cannot know the lamp sits at the back of
// a pocket: only tracing the light and letting the notch's own walls block
// it puts the spill where the housing allows it. The range bounds both which
// faces receive light and which triangles are tested as occluders, which is
// what keeps the shadow rays cheap -- past it the inverse square has taken
// the contribution below a display step anyway.
static constexpr float   s_kLedRangeMm   = 130.0f;
static constexpr float   s_kLedRefMm     = 22.0f;    // distance lit at full strength
static constexpr float   s_kLedGain      = 1.15f;
static constexpr float   s_kLedRayNear   = 0.02f;    // ray t window: skip the
static constexpr float   s_kLedRayFar    = 0.985f;   // receiver and the lens

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
static constexpr float   s_kMon2BrandTopZMm   = 46.0f;
static constexpr float   s_kMon2BrandHeightMm = 24.0f;
static constexpr float   s_kMon2BrandFrontY   = -0.8f;

// The drive's cassowary, lower-right of the faceplate like the 2D widget,
// proud of the black plate (front y = -1).
static constexpr float   s_kDriveBrandLeftMm   = 127.0f;
static constexpr float   s_kDriveBrandTopZMm   = 38.0f;
static constexpr float   s_kDriveBrandHeightMm = 29.0f;
static constexpr float   s_kDriveBrandFrontY   = -1.8f;

// The IN-USE label: "IN USE" plus the pointer triangle, sitting to the
// LED's left at the LED's height (the 2D widget's arrangement).
static constexpr float   s_kInUseLeftMm  = 20.0f;
static constexpr float   s_kInUseTopZMm  = 21.8f;
static constexpr float   s_kInUseCellMm  = 1.0f;
static constexpr float   s_kInUseFrontY  = -1.8f;
static constexpr float   s_kInUseRgb[3]  = { 0.750f, 0.730f, 0.700f };

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
//  Per-face lighting baked on the CPU from the room's two ceiling fixtures:
//  Lambert per light, two-sided (|dot|) since the meshes are drawn with
//  culling off, attenuated by inverse-square falloff and summed. The shade
//  premultiplies into the vertex tint; the pixel shader is tex*col and the
//  untextured path samples opaque white, so tint IS the lit color. Light
//  positions were moved into model space by Load(), so face math stays in
//  the mesh's own coordinates.
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
        Dxui3DRenderer::Vertex   v     = {};
        float                    sum   = 0.0f;
        float                    shade = s_kShadeFloor + s_kShadeSpan;

        // Sampled at the VERTEX, not the face's center. These are point
        // lights, so distance and angle vary across a face -- evaluating
        // once per face gives the two triangles of a flat quad two
        // different shades and draws their shared edge as a crease running
        // corner to corner. The face's own normal still drives the Lambert
        // term, so a flat surface stays flat; only the falloff varies, and
        // it now varies continuously across the seam.
        if (nl > 0.0f)
        {
            for (const float * light : m_lightsModel)
            {
                float  toL[3] = { light[0] - p[0], light[1] - p[1], light[2] - p[2] };
                float  r      = std::sqrt (toL[0] * toL[0] + toL[1] * toL[1] + toL[2] * toL[2]);

                if (r > 0.0f)
                {
                    float  d = (n[0] * toL[0] + n[1] * toL[1] + n[2] * toL[2]) / (nl * r);

                    sum += std::abs (d) * (s_kLightRefMm * s_kLightRefMm) / (r * r);
                }
            }

            shade = s_kShadeFloor + s_kShadeSpan * (std::min) (1.0f, sum);
        }

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
//  DeskSceneModel::RayHitsTriangle
//
//  Moller-Trumbore, used purely as an occlusion test: the caller only needs
//  to know whether anything stands between a face and the lamp, never where.
//  Hits count strictly INSIDE the segment, so the receiving face at one end
//  and the lens at the other cannot shadow the ray they define.
//
////////////////////////////////////////////////////////////////////////////////

bool DeskSceneModel::RayHitsTriangle (const float from[3], const float dir[3], const ObjTriangle & tri)
{
    float   e1[3] = { tri.p1[0] - tri.p0[0], tri.p1[1] - tri.p0[1], tri.p1[2] - tri.p0[2] };
    float   e2[3] = { tri.p2[0] - tri.p0[0], tri.p2[1] - tri.p0[1], tri.p2[2] - tri.p0[2] };
    float   h[3]  = { dir[1] * e2[2] - dir[2] * e2[1],
                      dir[2] * e2[0] - dir[0] * e2[2],
                      dir[0] * e2[1] - dir[1] * e2[0] };
    float   a     = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
    float   s[3]  = { from[0] - tri.p0[0], from[1] - tri.p0[1], from[2] - tri.p0[2] };
    float   q[3]  = {};
    float   f     = 0.0f;
    float   u     = 0.0f;
    float   v     = 0.0f;
    float   t     = 0.0f;



    if (std::abs (a) < 1e-7f)
    {
        return false;
    }

    f = 1.0f / a;
    u = f * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]);

    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }

    q[0] = s[1] * e1[2] - s[2] * e1[1];
    q[1] = s[2] * e1[0] - s[0] * e1[2];
    q[2] = s[0] * e1[1] - s[1] * e1[0];
    v    = f * (dir[0] * q[0] + dir[1] * q[1] + dir[2] * q[2]);
    t    = f * (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]);

    return v >= 0.0f && u + v <= 1.0f && t > s_kLedRayNear && t < s_kLedRayFar;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::BakeLampSpill
//
//  Builds the lamp-lit copy of the body: the room bake plus the light the
//  lamp itself throws, traced from the lens to every face near it and tested
//  against the housing in between. That test is the whole point -- the lens
//  sits at the back of the power notch, so the notch's own walls decide how
//  far the spill reaches and which surfaces stay dark, which a glow drawn
//  over the lens can never express.
//
//  The lamp's OWN color drives it, so the monitor's green and the drive's
//  red each tint their housing. The scene picks this copy or the plain one
//  by lamp state, so an unlit lamp throws nothing.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::BakeLampSpill (const ObjTriangle * tris,     size_t triCount,
                                    const size_t      * opaqueIdx, size_t opaqueCount,
                                    const float         lampKd[3])
{
    std::vector<const ObjTriangle *>   occluders;
    float                              center[3] = {};
    float                              dir[3]    = {};
    float                              rgb[3]    = {};
    float                              dl        = 0.0f;
    float                              count     = (float) m_lamp.size();



    m_opaqueLamp.clear();

    if (m_lamp.empty() || opaqueCount == 0)
    {
        return;
    }

    // The emitter stands in for the lens: where it sits, which way its face
    // looks, and what color it burns.
    for (const Dxui3DRenderer::Vertex & v : m_lamp)
    {
        center[0] += v.x / count;
        center[1] += v.y / count;
        center[2] += v.z / count;
        rgb[0]    += v.r / count;
        rgb[1]    += v.g / count;
        rgb[2]    += v.b / count;
    }

    for (size_t i = 0; i + 2 < m_lamp.size(); i += 3)
    {
        dir[0] += (m_lamp[i + 1].y - m_lamp[i].y) * (m_lamp[i + 2].z - m_lamp[i].z) -
                  (m_lamp[i + 1].z - m_lamp[i].z) * (m_lamp[i + 2].y - m_lamp[i].y);
        dir[1] += (m_lamp[i + 1].z - m_lamp[i].z) * (m_lamp[i + 2].x - m_lamp[i].x) -
                  (m_lamp[i + 1].x - m_lamp[i].x) * (m_lamp[i + 2].z - m_lamp[i].z);
        dir[2] += (m_lamp[i + 1].x - m_lamp[i].x) * (m_lamp[i + 2].y - m_lamp[i].y) -
                  (m_lamp[i + 1].y - m_lamp[i].y) * (m_lamp[i + 2].x - m_lamp[i].x);
    }

    dl = std::sqrt (dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);

    if (dl <= 0.0f)
    {
        return;
    }

    // Point it OUT of the case (-Y is toward the viewer): winding decides the
    // sign, and a lens wound the other way would fire into the cabinet.
    for (int i = 0; i < 3; i++)
    {
        dir[i] /= (dl * (dir[1] > 0.0f ? -1.0f : 1.0f));
    }

    // Everything solid near the lamp is a potential blocker -- except the
    // lens itself, which would shadow every ray it casts.
    for (size_t i = 0; i < triCount; i++)
    {
        if (ColorMatches (tris[i].r, tris[i].g, tris[i].b, lampKd))
        {
            continue;
        }

        if (TriangleNear (tris[i], center, s_kLedRangeMm))
        {
            occluders.push_back (&tris[i]);
        }
    }

    m_opaqueLamp = m_opaque;

    for (size_t i = 0; i < opaqueCount; i++)
    {
        AddLampSpill (tris[opaqueIdx[i]], center, dir, rgb, occluders, i * 3);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::TriangleNear
//
////////////////////////////////////////////////////////////////////////////////

bool DeskSceneModel::TriangleNear (const ObjTriangle & tri, const float point[3], float rangeMm)
{
    float   c[3] = { (tri.p0[0] + tri.p1[0] + tri.p2[0]) / 3.0f,
                     (tri.p0[1] + tri.p1[1] + tri.p2[1]) / 3.0f,
                     (tri.p0[2] + tri.p1[2] + tri.p2[2]) / 3.0f };
    float   d[3] = { c[0] - point[0], c[1] - point[1], c[2] - point[2] };



    return d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= rangeMm * rangeMm;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel::AddLampSpill
//
//  One face's share of the lamp: inverse-square falloff over the emitter's
//  own cosine (a lens lights what it faces) times the receiver's, dropped
//  entirely when the housing stands in the way. Added on top of the room
//  bake already in the vertex, so this reads as light ARRIVING rather than
//  as a repaint.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneModel::AddLampSpill (const ObjTriangle                      & tri,
                                   const float                              center[3],
                                   const float                              dir[3],
                                   const float                              rgb[3],
                                   const std::vector<const ObjTriangle *> & occluders,
                                   size_t                                   vertexBase)
{
    float   e1[3]  = { tri.p1[0] - tri.p0[0], tri.p1[1] - tri.p0[1], tri.p1[2] - tri.p0[2] };
    float   e2[3]  = { tri.p2[0] - tri.p0[0], tri.p2[1] - tri.p0[1], tri.p2[2] - tri.p0[2] };
    float   n[3]   = { e1[1] * e2[2] - e1[2] * e2[1],
                       e1[2] * e2[0] - e1[0] * e2[2],
                       e1[0] * e2[1] - e1[1] * e2[0] };
    float   nl     = std::sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    size_t  k      = 0;



    if (nl <= 0.0f)
    {
        return;
    }

    // Per vertex, for the same reason the room bake is: one sample at the
    // face's center steps at every shared edge, and a lamp this close to
    // its housing makes those steps obvious.
    for (const float * p : { tri.p0, tri.p1, tri.p2 })
    {
        float  toL[3] = { center[0] - p[0], center[1] - p[1], center[2] - p[2] };
        float  r      = std::sqrt (toL[0] * toL[0] + toL[1] * toL[1] + toL[2] * toL[2]);
        float  emit   = 0.0f;
        float  recv   = 0.0f;
        float  atten  = 0.0f;
        bool   shaded = false;

        if (r <= 0.0f || r > s_kLedRangeMm)
        {
            k++;
            continue;
        }

        // Behind the lens is dark: the emitter radiates into the hemisphere
        // its face looks at, which keeps light off the cabinet behind it.
        emit = -(dir[0] * toL[0] + dir[1] * toL[1] + dir[2] * toL[2]) / r;

        for (const ObjTriangle * blocker : occluders)
        {
            shaded = shaded || RayHitsTriangle (p, toL, *blocker);
        }

        if (emit > 0.0f && !shaded)
        {
            // Inverse square, but never nearer than the reference distance:
            // a lens is an area, not a point, and a true point source a few
            // millimeters off the notch floor divides by almost nothing --
            // the surface saturates to white and the lamp's color is the
            // thing that gets lost.
            recv  = std::abs (n[0] * toL[0] + n[1] * toL[1] + n[2] * toL[2]) / (nl * r);
            atten = emit * recv * s_kLedGain * (s_kLedRefMm * s_kLedRefMm) /
                    ((std::max) (r, s_kLedRefMm) * (std::max) (r, s_kLedRefMm));

            m_opaqueLamp[vertexBase + k].r += tri.r * rgb[0] * atten;
            m_opaqueLamp[vertexBase + k].g += tri.g * rgb[1] * atten;
            m_opaqueLamp[vertexBase + k].b += tri.b * rgb[2] * atten;
        }

        k++;
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
    m_opaqueLamp.clear();
    m_glass.clear();
    m_lamp.clear();
    m_door.clear();
    m_padlock.clear();
    m_lamps.clear();
    m_regions.clear();
    m_surface = {};

    lampKd = IsMonitorKind (kind) ? kMonitorLampKd : kDriveLampKd;

    hr = ObjMeshParser::Parse (objText, mtlText, triangles);
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
        const ObjTriangle &  tri = triangles[t];

        // Metadata first, and it never reaches a vertex buffer: the anchors
        // exist to be measured, not seen. Each names the midpoint of its own
        // extent, so any marker shape at all names the same line or plane.
        if (ColorMatches (tri.r, tri.g, tri.b, kBrandAnchorKd))
        {
            for (const float * p : { tri.p0, tri.p1, tri.p2 })
            {
                anchorLo = (std::min) (anchorLo, p[0]);
                anchorHi = (std::max) (anchorHi, p[0]);
            }

            continue;
        }

        if (ColorMatches (tri.r, tri.g, tri.b, kFrontAnchorKd))
        {
            for (const float * p : { tri.p0, tri.p1, tri.p2 })
            {
                frontLo = (std::min) (frontLo, p[1]);
                frontHi = (std::max) (frontHi, p[1]);
            }

            continue;
        }

        if (IsMonitorKind (kind) && ColorMatches (tri.r, tri.g, tri.b, kGlassKd))
        {
            AppendFlatTri (m_glass, tri);
        }
        else if (ColorMatches (tri.r, tri.g, tri.b, lampKd) ||
                 (kind == DeskDeviceKind::DiskII &&
                  ColorMatches (tri.r, tri.g, tri.b, kDriveLampAltKd)))
        {
            AppendFlatTri (m_lamp, tri);
        }
        else if (kind == DeskDeviceKind::DiskII &&
                 (ColorMatches (tri.r, tri.g, tri.b, kDriveDoorKd)     ||
                  ColorMatches (tri.r, tri.g, tri.b, kDriveLatchKd)    ||
                  ColorMatches (tri.r, tri.g, tri.b, kDriveDoorAltKd)  ||
                  ColorMatches (tri.r, tri.g, tri.b, kDriveLatchAltKd)))
        {
            AppendLitTri (m_door, tri);
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

            for (int row = 0; row < CassoBranding::kGridH; row++)
            {
                uint64_t  bits = CassoBranding::SilhouetteRow (row);

                for (int col = 0; col < CassoBranding::kGridW; col++)
                {
                    if ((bits & (1ULL << col)) != 0)
                    {
                        sumCol += (double) col + 0.5;
                        count++;
                    }
                }
            }

            {
                float  centroidCols = (count > 0) ? (float) (sumCol / count)
                                                  : (float) CassoBranding::kGridW * 0.5f;
                float  leftMm       = m_brandAxisX - centroidCols * cell;

                BuildBrandStamp (leftMm, s_kMon2BrandTopZMm,
                                 s_kMon2BrandHeightMm, s_kMon2BrandFrontY);
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
            anchor.frontY      = lo[1];                     // most proud (viewer at -Y)
            anchor.radiusX     = (hi[0] - lo[0]) * 0.5f;
            anchor.radiusZ     = (hi[2] - lo[2]) * 0.5f;
            anchor.firstVertex = 0;
            anchor.vertexCount = m_lamp.size();
        }

        m_lamps.push_back (anchor);
    }

    // Last, so the lit copy carries every proud stamp the body picked up
    // along the way -- the brand, the badges -- not just the mesh's own
    // triangles.
    BakeLampSpill (triangles.data(), triangles.size(),
                   opaqueTris.data(), opaqueTris.size(), lampKd);

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
