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
    Monitor2c,
    DiskII,
};


//
//  A lamp sub-mesh's placement: the tint applied at draw time is what turns
//  it on and off, so the model only records where it is and which triangles
//  belong to it (by range into LampVerts).
//
//  `frontY` is the lens face (the most proud vertex, viewer at -Y) and
//  `radius` half its larger in-plane extent -- between them the scene can
//  size and seat a glow disc on the lens without knowing the model.
//
struct DeskLampAnchor
{
    float   center[3]   = {};
    float   frontY      = 0.0f;
    float   radius      = 0.0f;
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
    const std::vector<Dxui3DRenderer::Vertex> &   OpaqueVerts  () const { return m_opaque; }
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

    // Sub-mesh identity colors, shared with scripts/modelgen/. Matching is
    // by value with kKdEpsilon, exactly as Printer3DScene matches its LEDs,
    // so a Tinkercad-refined model that keeps the palette keeps working.
    static constexpr float  kGlassKd[3]       = { 0.050f, 0.090f, 0.070f };
    static constexpr float  kMonitorLampKd[3] = { 0.290f, 0.870f, 0.380f };
    static constexpr float  kDriveLampKd[3]   = { 0.900f, 0.120f, 0.100f };
    static constexpr float  kDriveDoorKd[3]   = { 0.160f, 0.160f, 0.180f };
    static constexpr float  kDriveLatchKd[3]  = { 0.230f, 0.230f, 0.250f };
    static constexpr float  kKdEpsilon        = 0.02f;

    // Toward-viewer lift applied to the glass (verts + surface together) so
    // its corners never depth-tie with the cavity front they were generated
    // coplanar with.
    static constexpr float  kGlassLiftMm      = 0.6f;

private:
    static bool  ColorMatches   (float r, float g, float b, const float kd[3]);
    static void  AppendLitTri   (std::vector<Dxui3DRenderer::Vertex> & out, const struct ObjTriangle & tri);
    static void  AppendFlatTri  (std::vector<Dxui3DRenderer::Vertex> & out, const struct ObjTriangle & tri);

    HRESULT  BuildGlassSurface  ();
    void     AssignGlassUvs     ();
    void     BuildBrandStamp    (float leftMm, float topZMm, float heightMm, float frontY);
    void     BuildPadlockStamp  ();
    void     AddRegionBoxes     ();
    void     ComputeBounds      ();

    DeskDeviceKind                       m_kind         = DeskDeviceKind::Monitor2c;
    std::vector<Dxui3DRenderer::Vertex>  m_opaque;
    std::vector<Dxui3DRenderer::Vertex>  m_glass;
    std::vector<Dxui3DRenderer::Vertex>  m_lamp;
    std::vector<Dxui3DRenderer::Vertex>  m_door;
    std::vector<Dxui3DRenderer::Vertex>  m_padlock;
    std::vector<DeskLampAnchor>          m_lamps;
    std::vector<DeskRegionBox>           m_regions;
    CurvedDisplaySurface                 m_surface;
    float                                m_doorPivotY   = 0.0f;
    float                                m_doorPivotZ   = 0.0f;
    float                                m_boundsMin[3] = {};
    float                                m_boundsMax[3] = {};
};
