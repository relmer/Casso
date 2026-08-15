#pragma once

#include "Pch.h"

#include "Render/CurvedDisplayMath.h"
#include "Render/Dxui3DRenderer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneModel
//
//  One device kind's loaded mesh and everything discovered from it: sub-meshes
//  split by material color (glass, lamps), the curved display surface derived
//  from the glass geometry itself, synthesized glass UVs, and the model-space
//  interactive region boxes. Parsing and discovery are data-in/data-out over
//  OBJ/MTL text, so the whole load path is unit-testable with synthetic
//  buffers.
//
//  Vertices stay in the model's own space (X right, Y back from the front
//  face, Z up, millimeters); the scene's world matrices carry the Y-up axis
//  remap and placement. Sub-mesh identity is BY MATERIAL COLOR (the printer
//  scene's precedent): the generators and any Tinkercad-refined replacement
//  agree on Kd values, not on names -- the parser drops names entirely.
//
//  The sag sphere's radius is DERIVED from the measured glass mesh (rect,
//  corner plane, center depth) rather than assumed, so a regenerated or
//  hand-refined glass keeps input mapping exact as long as it remains a
//  spherical-sag sheet.
//
////////////////////////////////////////////////////////////////////////////////

enum class DriveWidgetRegion;


enum class DeskDeviceKind
{
    Monitor2c,     // the //c's platinum 9-inch
    Monitor2,      // the //e's beige 12-inch (Monitor II)
    DiskII,
};


//
//  Both monitors carry glass, a power lamp and a brand stamp, and differ only
//  in where those sit -- so the load path keys off "is a monitor" rather than
//  naming one of them, which is how the //e monitor first arrived wearing the
//  //c's brand position.
//
inline bool IsMonitorKind (DeskDeviceKind kind)
{
    return kind == DeskDeviceKind::Monitor2c || kind == DeskDeviceKind::Monitor2;
}


//
//  A lamp sub-mesh's placement: the tint applied at draw time is what turns
//  it on and off, so the model only records where it is and which triangles
//  belong to it (by range into LampVerts).
//
//  `frontY` is the lens face (the most proud vertex, viewer at -Y) and
//  `radiusX` / `radiusZ` its in-plane half-extents -- between them the scene
//  can seat a glow on the lens, shaped like it, without knowing the model.
//  Per axis rather than one radius because the lamps are not all round: the
//  //c-family power indicator is a tall narrow rhombus, and a circular glow
//  over it reads as a light behind a hole rather than a lit lens.
//
struct DeskLampAnchor
{
    float   center[3]   = {};
    float   frontY      = 0.0f;
    float   radiusX     = 0.0f;
    float   radiusZ     = 0.0f;
    size_t  firstVertex = 0;
    size_t  vertexCount = 0;
};


//
//  A model-space interactive region. Boxes are tested in declaration order,
//  so a box contained inside another (the slot inside the body) lists first
//  -- the same precedence DriveWidget::HitTest establishes for the 2D band.
//
struct DeskRegionBox
{
    float              boxMin[3] = {};
    float              boxMax[3] = {};
    DriveWidgetRegion  region    = {};
};


class DeskSceneModel
{
public:
    // Parses and splits the OBJ/MTL text. Monitor2c must carry exactly one
    // valid spherical-sag glass sheet; DiskII must carry its activity lamp.
    HRESULT  Load (DeskDeviceKind kind, const std::string & objText, const std::string & mtlText);

    DeskDeviceKind                                Kind         () const { return m_kind; }
    bool                                          HasGlass     () const { return !m_glass.empty(); }
    const CurvedDisplaySurface &                  Surface      () const { return m_surface; }
    // The body in its lamp-off or lamp-lit bake. The lit copy carries the
    // light the lamp throws on its own housing -- traced, so the power
    // notch's walls shape it. A model with no lamp has only the one copy.
    const std::vector<Dxui3DRenderer::Vertex> &   OpaqueVerts  (bool lampLit = false) const
    { return (lampLit && !m_opaqueLamp.empty()) ? m_opaqueLamp : m_opaque; }
    const std::vector<Dxui3DRenderer::Vertex> &   GlassVerts   () const { return m_glass; }
    const std::vector<Dxui3DRenderer::Vertex> &   LampVerts    () const { return m_lamp; }
    const std::vector<Dxui3DRenderer::Vertex> &   DoorVerts    () const { return m_door; }
    const std::vector<Dxui3DRenderer::Vertex> &   PadlockVerts () const { return m_padlock; }
    const std::vector<DeskLampAnchor> &           Lamps        () const { return m_lamps; }
    const std::vector<DeskRegionBox> &            RegionBoxes  () const { return m_regions; }

    // The door's hinge line: the X-axis line through the assembly's top-back
    // edge, where the real drive's flip-up door attaches to the faceplate.
    void  DoorPivot (float & outY, float & outZ) const { outY = m_doorPivotY; outZ = m_doorPivotZ; }

    // Rotates the cached door assembly about the hinge by `angleRad`: the
    // bottom edge swings out toward the viewer (-Y) and up, like the real
    // drive's door. Positions only -- the baked per-face shade rides along,
    // which reads fine over the door's small travel.
    static void  RotateDoorVerts (const std::vector<Dxui3DRenderer::Vertex> & base,
                                  float                                       pivotY,
                                  float                                       pivotZ,
                                  float                                       angleRad,
                                  std::vector<Dxui3DRenderer::Vertex>       & out);

    // Stamps text in a blocky 5x7 pixel font as proud unlit quads, one
    // merged quad per horizontal pixel run -- the same technique as the
    // brand stamp, so labels keep the period pixel-art house style. Glyphs
    // cover what the scene labels need (DRIVE n / IN USE and the '>'
    // LED-pointer triangle); unknown characters stamp as spaces. `cellMm`
    // is one font pixel; a glyph is 5 cells wide + 1 cell of tracking.
    static void  StampText (std::vector<Dxui3DRenderer::Vertex> & out,
                            const char                          * text,
                            float                                 leftMm,
                            float                                 topZMm,
                            float                                 cellMm,
                            float                                 frontY,
                            const float                           rgb[3]);

    void  BoundsMin (float out[3]) const { memcpy (out, m_boundsMin, sizeof (m_boundsMin)); }
    void  BoundsMax (float out[3]) const { memcpy (out, m_boundsMax, sizeof (m_boundsMax)); }

    // Model y of the FRAME's front face -- what devices stand flush with.
    // Deliberately not BoundsMin's y: this monitor's bezel protrudes an inch
    // past its frame and its tube bulges past even that, and lining the drives
    // up with the frontmost point marched them most of an inch forward of
    // where they belong. See kFrontAnchorKd.
    float  FrontPlaneY () const { return m_frontPlaneY; }

    // The XY rect of the geometry resting on the ground plane (z ==
    // BoundsMin's z), which is where a contact shadow belongs -- overhanging
    // bezels and lips are excluded.
    void  FootprintMin (float out[2]) const { memcpy (out, m_footprintMin, sizeof (m_footprintMin)); }
    void  FootprintMax (float out[2]) const { memcpy (out, m_footprintMax, sizeof (m_footprintMax)); }

    // How far above the lowest vertex still counts as touching the ground:
    // the larger of an absolute floor and a fraction of the model's height.
    // The absolute part catches a foot pad's top face on a small device; the
    // proportional part is there because a case can TAPER -- the Monitor //c
    // shell rises 6 mm from its front edge to its back, so a 2.5 mm band
    // found only the front edge and reported a footprint one line deep.
    static constexpr float  kGroundBandMm       = 2.5f;
    static constexpr float  kGroundBandFraction = 0.045f;

    // Sub-mesh identity colors, shared with scripts/modelgen/. Matching is
    // by value with kKdEpsilon, exactly as Printer3DScene matches its LEDs,
    // so a Tinkercad-refined model that keeps the palette keeps working.
    static constexpr float  kGlassKd[3]       = { 0.050f, 0.090f, 0.070f };
    static constexpr float  kMonitorLampKd[3] = { 0.290f, 0.870f, 0.380f };
    static constexpr float  kDriveLampKd[3]   = { 0.900f, 0.120f, 0.100f };
    static constexpr float  kDriveDoorKd[3]   = { 0.160f, 0.160f, 0.180f };
    static constexpr float  kDriveLatchKd[3]  = { 0.230f, 0.230f, 0.250f };
    static constexpr float  kKdEpsilon        = 0.02f;

    // Placement METADATA rather than scenery: a marker buried in the case
    // that names the axis the brand mark centers on. The mark itself cannot
    // live in the mesh -- it is a multi-color stamp, and identity here is one
    // Kd per part -- but its position can, and belongs there: it moves
    // whenever the reveal is resized, and a copy of that number kept in this
    // file drifted off the axis every single time. Triangles wearing this
    // color are read and DISCARDED, never drawn.
    static constexpr float  kBrandAnchorKd[3] = { 0.980f, 0.010f, 0.640f };

    // Likewise for the FRAME's front plane, which no amount of measuring the
    // mesh recovers: a monitor's bezel or faceplate stands proud of the frame
    // on purpose, and the model origin is not the frame either (the //c case
    // front sits a centimeter behind its plate). Only the generator knows.
    static constexpr float  kFrontAnchorKd[3] = { 0.980f, 0.010f, 0.240f };

    // The platinum-era drives (the //c's 5.25 unit) wear the same PARTS in
    // different colors: a green in-use lamp instead of red, and a door bar
    // and latch in case-colored plastic instead of black. Identity here is
    // by color, so those parts need identities of their own -- without them
    // a platinum drive loads as a case with no door and no lamp, which the
    // asset guards reject outright. Each stays clear of every other entry by
    // more than kKdEpsilon on at least one channel; the green in particular
    // is deliberately off the monitor lamp's, which it would otherwise be
    // mistaken for.
    static constexpr float  kDriveLampAltKd[3]  = { 0.250f, 0.845f, 0.330f };
    static constexpr float  kDriveDoorAltKd[3]  = { 0.720f, 0.712f, 0.685f };
    static constexpr float  kDriveLatchAltKd[3] = { 0.640f, 0.632f, 0.605f };

    // Toward-viewer lift applied to the glass (verts + surface together) so
    // its corners never depth-tie with the cavity front they were generated
    // coplanar with.
    static constexpr float  kGlassLiftMm      = 0.6f;

private:
    static bool  ColorMatches   (float r, float g, float b, const float kd[3]);
    static bool  RayHitsTriangle (const float from[3], const float dir[3], const struct ObjTriangle & tri);
    static bool  TriangleNear    (const struct ObjTriangle & tri, const float point[3], float rangeMm);

    void  BakeLampSpill (const struct ObjTriangle * tris,      size_t triCount,
                         const size_t             * opaqueIdx, size_t opaqueCount,
                         const float                lampKd[3]);
    void  AddLampSpill  (const struct ObjTriangle                      & tri,
                         const float                                     center[3],
                         const float                                     dir[3],
                         const float                                     rgb[3],
                         const std::vector<const struct ObjTriangle *> & occluders,
                         size_t                                          vertexBase);
    void         AppendLitTri   (std::vector<Dxui3DRenderer::Vertex> & out, const struct ObjTriangle & tri);
    static void  AppendFlatTri  (std::vector<Dxui3DRenderer::Vertex> & out, const struct ObjTriangle & tri);

    HRESULT  BuildGlassSurface  ();
    void     AssignGlassUvs     ();
    void     BuildBrandStamp    (float leftMm, float topZMm, float heightMm, float frontY);
    void     BuildPadlockStamp  ();
    void     AddRegionBoxes     ();
    void     ComputeBounds         ();
    void     ComputeGroundFootprint ();

    DeskDeviceKind                       m_kind            = DeskDeviceKind::Monitor2c;
    std::vector<Dxui3DRenderer::Vertex>  m_opaque;
    std::vector<Dxui3DRenderer::Vertex>  m_opaqueLamp;
    std::vector<Dxui3DRenderer::Vertex>  m_glass;
    std::vector<Dxui3DRenderer::Vertex>  m_lamp;
    std::vector<Dxui3DRenderer::Vertex>  m_door;
    std::vector<Dxui3DRenderer::Vertex>  m_padlock;
    std::vector<DeskLampAnchor>          m_lamps;
    std::vector<DeskRegionBox>           m_regions;
    CurvedDisplaySurface                 m_surface;
    float                                m_doorPivotY      = 0.0f;
    float                                m_doorPivotZ      = 0.0f;
    float                                m_boundsMin[3]    = {};
    float                                m_boundsMax[3]    = {};
    float                                m_footprintMin[2] = {};
    float                                m_footprintMax[2] = {};
    float                                m_lightsModel[2][3] = {};
    float                                m_brandAxisX        = 0.0f;
    float                                m_frontPlaneY       = 0.0f;
};
