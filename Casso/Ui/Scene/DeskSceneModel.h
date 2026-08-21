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
    // The body, once. The lamp used to need a second copy of it with the
    // spill traced from the lens and baked into the vertices; the lamp is
    // a real light in the shader now, so there is one body and the scene
    // switches the light instead.
    const std::vector<Dxui3DRenderer::Vertex> &   OpaqueVerts  () const { return m_opaque; }
    const std::vector<Dxui3DRenderer::Vertex> &   GlassVerts   () const { return m_glass; }
    const std::vector<Dxui3DRenderer::Vertex> &   LampVerts    () const { return m_lamp; }
    const std::vector<Dxui3DRenderer::Vertex> &   DoorVerts    () const { return m_door; }
    const std::vector<Dxui3DRenderer::Vertex> &   PadlockVerts () const { return m_padlock; }
    const std::vector<DeskLampAnchor> &           Lamps        () const { return m_lamps; }
    const std::vector<DeskRegionBox> &            RegionBoxes  () const { return m_regions; }

    // The Disk II door's motion is a CANTILEVER, not a hinge. It rises as it
    // swings, which is the only way it clears into a notch shallower than the
    // door is long: a pure pivot at the door's top would sweep its bottom edge
    // 57.7 mm back, and the notch is 38 deep. That arithmetic rules a simple
    // hinge out before any of it is modeled.
    //
    // Any planar displacement is still a rotation about SOME point, so one
    // rotation serves -- the pole is simply not on the part. Solved from the
    // two poses the photographs give: closed with the bottom edge at the
    // frame's bottom on the face, open with it risen to the notch's top and
    // barely proud of it, which is the "only the bottom edge and a few mm
    // show" pose. The pole lands inside the drive, where a linkage's would.
    //
    // Pole and angle live TOGETHER, and public, because neither means anything
    // without the other and a test can then assert the POSE the pair produces.
    // They were in two files, with the angle tuned in one while the pivot was
    // derived from the door's bounding box in the other -- so remodeling the
    // door silently moved the mechanism.
    static constexpr float  kDiskIiDoorPoleY   = 22.19f;
    static constexpr float  kDiskIiDoorPoleZ   = 65.37f;
    static constexpr float  kDiskIiDoorOpenRad = 1.3631f;

    // The door's pole: the X-axis line it TURNS ABOUT. Not a hinge, and not on
    // the part -- the mechanism is a cantilever, so the door rises as it swings
    // and the center of that motion sits inside the drive.
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

    // Stamps text in a 7x9 pixel font as proud unlit quads, one merged quad
    // per horizontal pixel run -- the same technique as the brand stamp, so
    // labels keep the period pixel-art house style. Glyphs cover what the
    // scene labels need (DRIVE n / IN USE and the '>' LED-pointer triangle);
    // unknown characters stamp as spaces. `cellMm` is one font pixel; a
    // glyph is 7 cells wide + 1 cell of tracking.
    //
    // The grid was 5x7, which is too few columns to carry a letterform with
    // any weight -- the labels read as chunky approximations of type rather
    // than type. Callers scale cellMm DOWN to match: the same label is now
    // more, smaller pixels rather than the same pixels made bigger.
    // Stamps a silhouette mask onto the LID -- the X/Y plane facing +Z --
    // rather than onto a front face. One merged quad per horizontal run, the
    // same technique as the other stamps.
    //
    // Rows run FRONT-TO-BACK: row 0 lands furthest from the viewer, so a mark
    // whose row 0 is its top reads right way up to someone standing at the
    // machine. `rowRgb` is three floats per row, which is what lets one
    // routine serve a single-color logotype and a striped cassowary alike.
    static void  StampTopMask (std::vector<Dxui3DRenderer::Vertex> & out,
                               const char * const                  * rows,
                               int                                   rowCount,
                               int                                   colCount,
                               float                                 leftMm,
                               float                                 backYMm,
                               float                                 cellMm,
                               float                                 topZ,
                               const float                         * rowRgb);

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

    // WHICH PART a sub-mesh IS, by the material name the generator wrote --
    // `m.add("door", ...)` becomes `newmtl door` becomes this.
    //
    // Identity used to be the Kd VALUE, which made a color mean two things at
    // once. Two parts could not share a shade; a part could not be recolored
    // without becoming a different part (a diagnostic recolor broke the load
    // outright); and the door had to wear an identity color instead of the
    // pebbled black it is actually molded in, so its finish had to be
    // reapplied in the loader. Names separate the two: this says WHAT, and Kd
    // is free to say what it looks like.
    static constexpr const char *  s_kpszGlass       = "glass";
    static constexpr const char *  s_kpszLamp        = "lamp";
    static constexpr const char *  s_kpszLed         = "led";
    static constexpr const char *  s_kpszDoor        = "door";
    static constexpr const char *  s_kpszLever       = "lever";   // the //c's door
    static constexpr const char *  s_kpszTab         = "tab";     // the //c's latch
    static constexpr const char *  s_kpszBrandAnchor = "brand_anchor";
    static constexpr const char *  s_kpszFrontAnchor = "front_anchor";

    // Sub-mesh identity colors, shared with scripts/modelgen/. Matching is
    // by value with kKdEpsilon, exactly as Printer3DScene matches its LEDs,
    // so a Tinkercad-refined model that keeps the palette keeps working.
    //
    // Only the FINISH markers below are still read this way. The identity
    // ones are kept for the moment because Printer3DScene still matches its
    // own models by color.
    static constexpr float  kGlassKd[3]       = { 0.050f, 0.090f, 0.070f };
    static constexpr float  kMonitorLampKd[3] = { 0.290f, 0.870f, 0.380f };
    static constexpr float  kDriveLampKd[3]   = { 0.900f, 0.120f, 0.100f };
    static constexpr float  kDriveDoorKd[3]   = { 0.160f, 0.160f, 0.180f };

    // Black plastic with the molded pebble grain. A FINISH marker, not a
    // color: Load forces the tint back to the matte plate's own black and
    // raises the per-vertex pebble flag instead, so the two finishes differ
    // only in how they take light. Same trick the glass uses to be white
    // whatever Kd identified it.
    static constexpr float  kPlatePebbledKd[3] = { 0.135f, 0.130f, 0.150f };
    static constexpr float  kPlateMatteRgb[3]  = { 0.100f, 0.100f, 0.110f };
    static constexpr float  kDriveLatchKd[3]   = { 0.230f, 0.230f, 0.250f };
    static constexpr float  kKdEpsilon         = 0.02f;

    // The room's shading ramp, public because the SHADER applies it now and
    // the scene has to hand these to the renderer. The floor is deliberately
    // low: a shallow ramp reads as a flat 2D cutout. The reference distance
    // normalizes the inverse-square falloff so a face at ceiling height under
    // a fixture sits at full span.
    static constexpr float  kShadeFloor       = 0.16f;
    static constexpr float  kShadeSpan        = 0.84f;
    static constexpr float  kLightRefMm       = 1524.0f;

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
    void         AppendLitTri   (std::vector<Dxui3DRenderer::Vertex> & out, const struct ObjTriangle & tri);
    static void  AppendFlatTri  (std::vector<Dxui3DRenderer::Vertex> & out, const struct ObjTriangle & tri);

    HRESULT  BuildGlassSurface  ();
    void     AssignGlassUvs     ();
    void     BuildBrandStamp    (float leftMm, float topZMm, float heightMm, float frontY,
                                 float thicknessMm = 0.0f);
    void     BuildBrandSolid    (float leftMm, float topZMm, float heightMm, float frontY,
                                 float thicknessMm, int firstRow, int lastRow);
    void     BuildPadlockStamp  ();

    // The lid's printed marking: the "disk ][" logotype and the cassowary,
    // laid on the metal rather than on a face.
    void     BuildLidLabel      ();
    void     AddRegionBoxes     ();
    void     ComputeBounds         ();
    void     ComputeGroundFootprint ();

    DeskDeviceKind                       m_kind            = DeskDeviceKind::Monitor2c;
    std::vector<Dxui3DRenderer::Vertex>  m_opaque;
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

public:
    // The room fixtures in THIS model's coordinates, for the caller to hand
    // to the renderer before drawing it. Each device sits somewhere different
    // on the desk, so every one sees the same two ceiling lights from its own
    // position -- which is the whole reason the scene cannot set lighting once
    // per frame and forget it.
    const float (&LightsModel() const)[2][3]  { return m_lightsModel; }

private:
    float                                m_brandAxisX        = 0.0f;
    float                                m_frontPlaneY       = 0.0f;
};
