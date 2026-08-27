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


//
//  How a drive's door gets out of the way. The two are not one motion with
//  different numbers: the Disk II's door turns about a pole that is not on
//  the part, and the //c's latch travels in a straight line and then another
//  straight line. Naming both stops the scene assuming every door rotates,
//  which is the assumption that made the //c's latch sweep out of its case.
//
enum class DeskDoorMotion
{
    Cantilever,    // the Disk II: one rotation about a pole inside the drive
    InThenUp,      // the //c: back into the case, then straight up
};


enum class DeskDeviceKind
{
    Monitor2c,     // the //c's platinum 9-inch
    Monitor2,      // the //e's beige 12-inch (Monitor II)
    DiskII,
    Disk2c,        // the platinum 5.25 that pairs with the //c
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
//  Both drives carry a door the scene swings on eject, an activity lamp and a
//  write-protect badge, and differ in everything else -- the //c's is 70 mm
//  tall against the Disk II's 95, and its face already carries its own brand,
//  lamp and lever in the MESH rather than wanting them stamped on.
//
//  Which is why they need separate kinds. Loading the //c as a Disk II gave
//  it the Disk II's marks at the Disk II's coordinates: DRIVE n and the
//  padlock landed above a face 20 mm shorter than the one they were placed
//  against and printed on the monitor standing on it, the cassowary hung off
//  the right edge, and a "disk ][" logotype appeared on a drive that never
//  wore one.
//
inline bool IsDriveKind (DeskDeviceKind kind)
{
    return kind == DeskDeviceKind::DiskII || kind == DeskDeviceKind::Disk2c;
}


//
//  A lamp sub-mesh's placement: the tint applied at draw time is what turns
//  it on and off, so the model only records where it is and which triangles
//  belong to it (by range into LampVerts).
//
//  `frontY` is the lens face (the most proud vertex, viewer at -Y), which is
//  what the scene stands the LIGHT off -- see kLampLightStandoffMm. And
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

    // Which way the PANEL the lamp is set into faces, taken from the nearest
    // body triangle behind it. The glow is a flat disc, and a flat disc laid
    // on a sloping panel gets sliced by it -- which is what put a hard
    // horizontal edge across the //c monitor's halo, at exactly the height
    // where the leaning frame overtook the disc's plane. Lay the disc IN the
    // panel and there is nothing to slice it.
    float   facing[3]   = { 0.0f, -1.0f, 0.0f };
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

    // The Disk II faceplate's size, which is the case's outside less the
    // sheet that wraps it -- 6.125 x 3.625 in measured off a real drive.
    //
    // NAMED, because everything on the front is placed relative to an edge of
    // it and the numbers had been written out as literals in three files. A
    // case that turned out to be seven percent too tall then had to be
    // corrected in every one of them, and any that was missed would have put
    // a mark off the plate without anything failing to build.
    static constexpr float  kFaceWidthMm  = 153.075f;
    static constexpr float  kFaceHeightMm =  89.575f;

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
    static constexpr float  kDiskIiDoorPoleY   = 20.826f;
    static constexpr float  kDiskIiDoorPoleZ   = 60.828f;
    static constexpr float  kDiskIiDoorOpenRad = 1.3631f;

    // THE //c's LATCH DOES NOT TURN AT ALL. It travels: in toward the rear of
    // the drive, then straight up. No tilt, no pivot, no arc.
    //
    // Which is why it went through two wrong mechanisms before this one --
    // both of them rotations, because the only door the scene knew how to
    // move was the Disk II's, and a rotation is what that one is. Given a
    // hinge at an edge it swept out of the case; given a pivot through its
    // middle it tilted like a flap. The part does neither. A rotation cannot
    // express this however its pole is placed, so the motion has to be a
    // choice the model makes rather than something every door shares.
    //
    // The IN leg is LATCH_TRAVEL_IN from cad_disk2c.py, which sizes the notch
    // to be that much deeper than the latch is thick. The two are one fact in
    // two files, so they are named after each other: change either without
    // the other and the latch travels through the back of its own notch.
    static constexpr float  kDisk2cDoorInMm  = 4.0f;
    static constexpr float  kDisk2cDoorUpMm  = 13.0f;

    // And BECAUSE it rises, the row has to stand clear of whatever is stacked
    // on it. The latch travels up past the lid, and this scene puts a monitor
    // there -- so the open latch would pass through it. Far enough forward
    // that the whole of the latch's open position is in front of the
    // monitor's face: the latch reaches about 7 mm behind the drive's own
    // front plane when open, plus margin.
    //
    // This is the one thing that breaks the stack's shared front plane, and
    // it breaks it for a reason a photograph cannot show -- the drives in the
    // reference sit BESIDE the computer, where nothing is above them and
    // nothing has to move out of the way.
    static constexpr float  kDisk2cDoorFrontClearMm = 10.0f;

    // How far in front of anything stacked on it this drive's row must sit.
    float  DoorFrontClearanceMm () const
    {
        return (m_doorMotion == DeskDoorMotion::InThenUp) ? kDisk2cDoorFrontClearMm : 0.0f;
    }

    // And how far above its own lid the open door reaches, which is how much
    // air whatever stands on this drive has to leave it.
    float  DoorRiseMm () const
    {
        return (m_doorMotion == DeskDoorMotion::InThenUp) ? kDisk2cDoorUpMm : 0.0f;
    }

    // The door's pole: the X-axis line it TURNS ABOUT. Not a hinge, and not on
    // the part -- the mechanism is a cantilever, so the door rises as it swings
    // and the center of that motion sits inside the drive.
    void  DoorPivot (float & outY, float & outZ) const { outY = m_doorPivotY; outZ = m_doorPivotZ; }

    // How far it swings, WITH the pole -- meaningless apart, so kept together
    // and only meaningful for a Cantilever door.
    float  DoorOpenRad () const { return m_doorOpenRad; }

    DeskDoorMotion  DoorMotion () const { return m_doorMotion; }

    // The door at `progress` (0 shut, 1 open), by whichever motion this drive
    // actually has. One call, so a caller cannot pose a sliding latch as a
    // turning one by reaching for the rotation because it is the one it knows.
    void  PoseDoor (float progress, std::vector<Dxui3DRenderer::Vertex> & out) const;

    // Straight-line travel: back into the case over the first half, then up
    // over the second. Two legs rather than a diagonal because that is what
    // the part does -- it has to clear before it can rise.
    static void  SlideDoorVerts (const std::vector<Dxui3DRenderer::Vertex> & base,
                                 float                                       inMm,
                                 float                                       upMm,
                                 float                                       progress,
                                 std::vector<Dxui3DRenderer::Vertex>       & out);

    // Rotates the cached door assembly about the hinge by `angleRad`: the
    // bottom edge swings out toward the viewer (-Y) and up, like the real
    // drive's door. Positions only -- the baked per-face shade rides along,
    // which reads fine over the door's small travel.
    static void  RotateDoorVerts (const std::vector<Dxui3DRenderer::Vertex> & base,
                                  float                                       pivotY,
                                  float                                       pivotZ,
                                  float                                       angleRad,
                                  std::vector<Dxui3DRenderer::Vertex>       & out);

    // Stamps a silhouette mask onto a FRONT FACE, one merged quad per
    // horizontal run -- the same technique as the brand stamp, for marks that
    // are shapes rather than letters. `rowRgb` is three floats per row, which
    // is what lets one routine serve a single-color logotype and a striped
    // mark alike.
    //
    // `lit` decides whether the mark takes the room's light. Printed ink
    // wants it off, so a brand's colors come out exact; anything meant to
    // read as METAL wants it on, because a highlight that does not move with
    // the surface is not a highlight, and the drive's legends are silver.
    static void  StampFaceMask (std::vector<Dxui3DRenderer::Vertex> & out,
                                const char * const                  * rows,
                                int                                   rowCount,
                                int                                   colCount,
                                float                                 leftMm,
                                float                                 topZMm,
                                float                                 cellMm,
                                float                                 frontY,
                                const float                         * rowRgb,
                                bool                                  lit);

    // Stamps "DRIVE n" onto the faceplate, from the baked masks. The scene
    // owns these because they differ per drive and the model is shared.
    static void  StampDriveLabel (std::vector<Dxui3DRenderer::Vertex> & out, int driveNumber);

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
    static constexpr const char *  s_kpszAcPinPrefix = "acpin";   // the mains blades
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

    // The same grained plastic, DOWN A POCKET. A recess sees less of the room
    // than the face around it, and nothing in this renderer works that out on
    // its own -- there is no ambient occlusion, so a molded-in cavity comes
    // out exactly as bright as the surface it is cut into. Which is what left
    // the door's grip with nothing to be a silhouette against: the recess
    // behind it was the same value it was.
    //
    // A second finish marker rather than a darker Kd on the same one, because
    // the pebbled marker's whole job is to force one tint and raise the
    // grain flag; a recess needs the flag AND a different tint.
    // A FIFTH of the face's black. Measured down the door: the grip's face
    // lands around 15 of 255 along its bottom edge, and the pocket behind it
    // sat at 9 -- a ratio of 1.6 at values that low is not a boundary the eye
    // finds, which is why the grip kept dissolving into the recess.
    //
    // The grip's TOP edge reads because its chamfer faces the fixtures and
    // peaks near 40. Its bottom edge faces away from them and always will --
    // a chamfer, a round-over, any edge treatment at all still points down,
    // and down is where the light is not. No geometry fixes that. What has to
    // carry the bottom edge is the pocket being visibly deeper, which is also
    // what a photograph of the real drive shows: the finger recess is black.
    static constexpr float  kPlateRecessKd[3]  = { 0.115f, 0.112f, 0.132f };
    static constexpr float  kPlateRecessRgb[3] = { 0.013f, 0.013f, 0.015f };
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
    // The write-protect badge, its top-right corner where the caller says.
    // Both drives carry it and their faces are different heights, so the
    // corner is an argument rather than a constant.
    void     BuildPadlockStamp  (float rightX, float topZ, float frontY, float scale);

    // A mask stamped as a solid with a SMOOTHED outline and a rounded top
    // edge -- what turns a coarse bitmask into a mark that reads as molded
    // rather than as pixels.
    //
    // Offsetting the raw silhouette cannot do this: one cell is the same
    // order as any round-over worth having, so rounding by it would erase
    // single-cell features outright. The mask is RESAMPLED instead --
    // coverage over a disc a little wider than a cell, thresholded at half --
    // which rounds the staircase off while holding the mark's area and its
    // concavities, and the same field gives a distance to the outline, so cap
    // height can ramp down over the last fraction of a millimeter.
    //
    // `mask` is gridW * gridH bytes, row-major, nonzero for ink. `rowRgb` is
    // three floats per GRID row. `litFace` shades the flat interior along
    // with the relief; leave it off where exact ink values matter more than
    // the light.
    //
    // `smoothCells` is the coverage disc's radius IN MASK CELLS, and it is
    // the one number that has to suit the mark. A disc near a cell rounds a
    // staircase nicely, but it also closes any gap narrower than itself --
    // on a mark whose letters stand a cell apart, the default welds them into
    // a blob. The fix is resolution, not a smaller disc: give the mask finer
    // cells and the same radius covers less of the mark.
    //
    // `superSample` is fine cells per mask cell. It costs the square of
    // itself in both work and triangles, so a mask that arrives already fine
    // should lower it rather than pay twice.
    // A mask as a FLAT mark with a genuinely smooth outline -- no relief, no
    // thickness, for a logo printed on a surface rather than molded into it.
    //
    // The difference from BuildRelief is where the smoothing lands. That one
    // resamples the mask and then FILLS whole fine cells, so however smooth
    // the underlying field is, the boundary the eye sees is still a staircase
    // of cell-sized steps -- finer than the mask's, but a staircase. This one
    // finds where the field actually crosses its half level along each row,
    // by interpolating between samples, and puts the geometry's edge THERE.
    // The outline stops being quantized at all.
    //
    // Two things follow from that. The filter is a smooth falloff rather than
    // a hard disc, because a box filter's own edge puts kinks in the field
    // and a kink in the field is a kink in the contour. And consecutive rows
    // are joined as TRAPEZOIDS wherever their spans correspond, so a sloping
    // edge is one straight run instead of a stack of rectangles -- which is
    // what removes the last of the stepping.
    static void  BuildSmoothMask (std::vector<Dxui3DRenderer::Vertex> & out,
                                  const uint8_t * mask, int gridW, int gridH,
                                  float leftMm, float topZMm, float heightMm, float frontY,
                                  const float * rowRgb, bool lit,
                                  float smoothCells = 2.2f, int subdivide = 6);

    // The cassowary's silhouette and stripe colors as a mask plus one rgb per
    // grid row, which is what both mark builders take.
    static void  BrandMask      (std::vector<uint8_t> & outMask, std::vector<float> & outRgb,
                                 int firstRow, int lastRow);

    static void  BuildRelief    (std::vector<Dxui3DRenderer::Vertex> & out,
                                 const uint8_t * mask, int gridW, int gridH,
                                 float leftMm, float topZMm, float heightMm, float frontY,
                                 float thicknessMm, float rollMm,
                                 const float * rowRgb, bool litFace,
                                 float smoothCells = 1.15f, int superSample = 4);

    // "IN USE" and its pointer, printed beside the drive's lamp.
    void     BuildInUseStamp    ();

    // The "disk ][" logotype, embossed low on the faceplate.
    void     BuildWordmarkStamp ();
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
    float                                m_doorOpenRad     = 0.0f;
    DeskDoorMotion                       m_doorMotion      = DeskDoorMotion::Cantilever;

    // Where the write-protect badge actually landed, so its hit box is taken
    // from the badge rather than written out a second time beside it.
    float                                m_padlockMin[3]   = {};
    float                                m_padlockMax[3]   = {};
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
