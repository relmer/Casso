#pragma once

#include "Pch.h"

#include "Render/CurvedDisplayMath.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout
//
//  Computes the desk scene's composition -- device world transforms, the ONE
//  shared camera, and the scene scale -- from the viewport rect, DPI, drive
//  count, and the measured model metrics. Deterministic and GPU-free so the
//  composition rules (containment, drive count, position-derived perspective)
//  are unit-testable.
//
//  There is exactly one camera per composition (FR-016): every device is
//  truly placed in the one world, so a drive right of center is seen slightly
//  from its left, one below slightly from above -- the perspective IS the
//  placement, never a per-device effect. The world frame is X right, Y up,
//  Z toward the viewer; models (X right, Y back, Z up, front face at y=0)
//  are mounted by a fixed axis remap carried in each device's world matrix.
//
//  The composition echoes the 2D chrome it supersedes: monitor centered with
//  its front at z=0, drives on the ground plane in a forward row -- closer to
//  the viewer, so they read below the monitor exactly where the drive band
//  sat. The camera looks straight ahead at glass-center height (keeping the
//  glass front-facing for the display), and its standoff is solved in closed
//  form so the whole scene is contained at any viewport shape.
//
////////////////////////////////////////////////////////////////////////////////

//
//  Measured model dimensions the layout composes with -- passed in rather
//  than read from loaded models so tests can drive synthetic geometry.
//  All values are model space (mm).
//
struct DeskSceneMetrics
{
    float                 monitorMin[3] = {};
    float                 monitorMax[3] = {};
    float                 driveMin[3]   = {};
    float                 driveMax[3]   = {};
    CurvedDisplaySurface  glass;                 // the monitor's surface

    // Model y of each device's FRAME front -- the plane the stack lines up
    // on. Separate from the mins because a protruding bezel or faceplate is
    // not the frame: see DeskSceneModel::FrontPlaneY. Defaulting both to zero
    // leaves synthetic test geometry stacked on the origin plane, which is
    // what a plain box would report anyway.
    float                 monitorFrontY = 0.0f;
    float                 driveFrontY   = 0.0f;

    // How far FORWARD of the shared front plane the drive row must stand for
    // its door to open at all. Zero for a door that stays inside its own
    // case; the //c's latch rises above the lid, and this stack puts a
    // monitor there for it to hit.
    float                 driveDoorClearMm = 0.0f;

    // And how far ABOVE its own lid the same door reaches. A latch that rises
    // needs air over it as well as room in front, and what is directly over
    // the drives in this stack is the monitor's underside. Zero for a door
    // that stays inside its case.
    float                 driveDoorRiseMm  = 0.0f;

    // Ground-plane clearance each device's contact shadow needs, in that
    // device's model mm (side, front-to-back). The containment solve counts
    // it as part of the scene: a shadow lies on the floor BEYOND its device
    // and nearer the camera, so a scene contained to the devices alone
    // projects it straight off the bottom edge. Zero simply contains the
    // devices. Per device rather than one scene-wide pad because the monitor
    // needs a far longer forward shadow than the drives (it is seen nearly
    // head-on, so its floor is edge-on) and it sits BEHIND the drive row --
    // charging the whole scene for the monitor's reach would shrink
    // everything to buy clearance the drives already provide.
    float                 monitorPadSideMm  = 0.0f;
    float                 monitorPadDepthMm = 0.0f;
    float                 drivePadSideMm    = 0.0f;
    float                 drivePadDepthMm   = 0.0f;
};


//
//  One frame's composition. World matrices carry the model->world axis remap
//  plus placement; viewProj is the single shared camera.
//
//
//  The user's own framing on top of the fitted composition: how far in they
//  have zoomed and where they have dragged the scene to.
//
//  This is deliberately NOT part of the containment solve. The solve answers
//  "what standoff shows the whole scene", and that answer should not change
//  because someone leaned in to look at a drive door -- so the zoom is a lens
//  applied AFTER it, in clip space, and the composition it magnifies is
//  always the same one.
//
//  Pan is in NDC, so it is resolution-independent by construction: 0.5 shifts
//  by a quarter of the viewport whatever the window size or DPI, and a window
//  resize cannot slide the scene out from under the user.
//
struct DeskSceneView
{
    float  zoom = 1.0f;    // 1 == the fitted composition
    float  panX = 0.0f;    // NDC, +right
    float  panY = 0.0f;    // NDC, +up

    // The inspection orbit: the MODELS turn about the gaze target, the eye
    // never moves. Yaw is about the world's up axis, pitch about the
    // screen's horizontal, and the room lights stay put in the world -- so
    // turning the scene changes what they light, which is the whole reason
    // the rotation lives on the models rather than the camera. Zero-zero is
    // the composed pose.
    float  orbitYawRad   = 0.0f;
    float  orbitPitchRad = 0.0f;

    bool  IsIdentity () const
    {
        return zoom == 1.0f && panX == 0.0f && panY == 0.0f &&
               orbitYawRad == 0.0f && orbitPitchRad == 0.0f;
    }
};


struct DeskSceneComposition
{
    float  view[16]         = {};
    float  proj[16]         = {};
    float  viewProj[16]     = {};
    RECT   viewportPx       = {};
    float  monitorWorld[16] = {};
    float  driveWorld[2][16] = {};
    int    driveCount       = 0;
    float  sceneScale       = 0.0f;   // glass px height / (384 dp at dpi)
    RECT   glassRectPx      = {};     // projected glass bounds -- the CRT target rect
    RECT   sceneRectPx      = {};     // projected scene bounds -- what the composition occupies
    RECT   monitorRectPx    = {};     // projected monitor bounds -- chrome lays out against its edges
    RECT   driveRectPx[2]   = {};     // projected per-drive bounds -- tooltip anchors + drop targets

    // Each drive's FRONT-BOTTOM CENTER projected to the screen: the fixed
    // model point the 2D name strip hangs from. A box's projected bounds
    // swell and swing as it turns, so chrome hung from them swims; one
    // point rides the drive rigidly through any orbit.
    POINT  driveLabelPx[2]  = {};

    // The same anchor in WORLD space, which is what a depth-tested billboard
    // needs and a screen point cannot give. The depth is the whole difficulty
    // here: the name has to sit at the drive's own distance before the
    // monitor's case can stand in front of it.
    float  driveLabelWorld[2][3] = {};
};


class DeskSceneLayout
{
public:
    // Deterministic for a given (viewport, dpi, driveCount, metrics).
    // Returns S_FALSE (composition zeroed) for an empty viewport, e.g.
    // minimized. `reservedGapPx` keeps at least that many pixels of
    // projected clearance between the monitor's bottom and the drive row --
    // the slot the fixed-height input-mode chrome sits in -- by deepening
    // the drive drop as the scene scales down.
    static HRESULT  Compute (const RECT             & viewportPx,
                             UINT                     dpi,
                             int                      driveCount,
                             const DeskSceneMetrics & metrics,
                             DeskSceneComposition   & out,
                             int                      reservedGapPx = 0,
                             const DeskSceneView    & view = DeskSceneView {});

    // Magnify and shift a projection in CLIP SPACE, which is what lets the
    // user's framing ride on top of the fitted composition without disturbing
    // it. Applied before the bounds are projected, so glassRectPx and the
    // drive rects follow the zoom automatically -- the CRT still lands on the
    // glass and clicks still hit the drive they look like they hit.
    //
    // Scaling the projection rather than dollying the camera is deliberate:
    // a dolly changes the perspective, so leaning in to inspect a part would
    // show you a differently-shaped part than the composition does.
    static void     ApplyViewTransform (const DeskSceneView & view, float proj[16]);

    // The fullscreen presentation: a straight-on camera standing as close as
    // it can WITHOUT cutting the picture, no drives in the composition (the
    // overlay strip presents those separately). Close means the glass covers
    // the viewport and the monitor body crops offscreen; a viewport wide
    // enough that covering would crop the raster instead backs the eye off to
    // the picture's own containment. `displayW` x `displayH` is the emulated
    // grid, which is what fixes the picture's band on the glass. Same S_FALSE
    // contract for an empty viewport.
    static HRESULT  ComputeGlassFill (const RECT             & viewportPx,
                                      UINT                     dpi,
                                      int                      displayW,
                                      int                      displayH,
                                      const DeskSceneMetrics & metrics,
                                      DeskSceneComposition   & out);

    // The fullscreen drive overlay strip: the drive row alone, full model
    // scale, its own contained camera over the strip's viewport band --
    // a composed presentation in its own right, so FR-016 applies within
    // it: each drive's perspective derives from its position under the
    // strip's single camera. Same S_FALSE contract for an empty viewport.
    static HRESULT  ComputeStrip (const RECT             & viewportPx,
                                  UINT                     dpi,
                                  int                      driveCount,
                                  const DeskSceneMetrics & metrics,
                                  DeskSceneComposition   & out,
                                  float                    gazeDownRad = kGazeDownRad);

    // The fixed model->world mount: X right stays, model Z (up) becomes world
    // Y, model Y (back) becomes world -Z, uniformly scaled by `scale`, then
    // translated by (tx, ty, tz).
    static void     MakeDeviceWorld (float tx, float ty, float tz, float scale, float out[16]);

    // The Ctrl+0 inverse: the center (viewport) size at which the emulator
    // picture -- aspect-fitted INSIDE the projected glass, like the image on
    // a real tube -- lands at the requested pixel size. The glass's own
    // aspect is fixed by the model, so the solve scales the trial center
    // uniformly (one shared factor) on the fitted height; glassPx(center)
    // is piecewise linear, so this converges within a pixel in a few passes.
    static SIZE     CenterSizeForDisplayPx (int                      displayWpx,
                                            int                      displayHpx,
                                            UINT                     dpi,
                                            int                      driveCount,
                                            const DeskSceneMetrics & metrics,
                                            int                      reservedGapPx = 0);

    // The native-scale display height the 2D chrome established; sceneScale
    // is glass px height relative to this at the given DPI.
    static constexpr int    kScreenNativeHDp   = 384;

    // Shared camera: vertical fov, near/far planes (mm). The far plane
    // covers the containment solve's worst case -- an extremely narrow
    // viewport pushes the camera a long way back before the scene fits.
    static constexpr float  kFovY              = 0.35f;
    static constexpr float  kNearMm            = 10.0f;
    static constexpr float  kFarMm             = 30000.0f;

    // Composition: a real desk, measured.
    //
    // Every device sits at TRUE SIZE on one desk surface, and the camera is a
    // seated person's eye -- kViewingDistanceMm back from the monitor's front
    // plane with the eye kEyeAboveMonitorTopMm above the monitor's top, which
    // is where a display gets set up. Nothing is scaled or dropped to taste:
    // the drives are seen from a steeper angle than the monitor purely
    // because they stand closer and lower, and the monitor reads nearly
    // head-on because the eye is barely above it. The perspective IS the
    // placement (FR-016), now all the way down to the numbers.
    //
    // The drive row's own distance is the one adjustable placement, and it is
    // still physical: sliding the drives forward on the desk is how you open
    // room between them and the monitor, which is what Compute's gap
    // correction does.
    static constexpr float  kViewingDistanceMm     = 762.0f;   // 30 in, eye to screen

    // How far the orbit may raise or sink the eye's elevation, total. Short
    // of the pole by a few degrees, because at the pole LookAt's fixed up
    // vector is parallel to the gaze and the basis collapses.
    static constexpr float  kOrbitMaxElevRad       = 1.48f;    // ~85 degrees
    static constexpr float  kEyeAboveMonitorTopMm  = 25.0f;
    static constexpr float  kDriveDeskGapMm        = 45.0f;    // monitor front to drive back

    static constexpr float  kDriveGapMm            = 32.0f;    // between the two drives

    // Air over a risen door, on top of the rise itself. Enough that the gap
    // reads as a gap rather than as two parts just failing to touch.
    static constexpr float  kDoorRiseMarginMm      =  6.0f;
    static constexpr float  kContainMargin         = 1.005f;

    // Field of view is SOLVED rather than fixed, because the eye position is
    // fixed. Where the camera stands sets the perspective; the fov only
    // decides how much of that view the window shows, so fitting the scene by
    // fov keeps the physical geometry exact at any window shape. Clamped so a
    // pathological viewport cannot produce a fisheye or a pinhole.
    static constexpr float  kMinFovY = 0.12f;
    static constexpr float  kMaxFovY = 1.30f;

    // Breathing room the Ctrl+0 shrink-wrap adds around the scene footprint.
    static constexpr int    kCenterPadPx       = 4;

    // Downward viewing angle: a person at a desk looks slightly down at the
    // hardware, which is also what reveals the top surfaces that make the
    // scene read as 3D instead of a flat front-on cutout.
    static constexpr float  kGazeDownRad       = 0.09f;

    // The drive row composed on its own -- the CRT monitor opted out -- is
    // seen from the angle the 2D drive widget DREW: its receding case top
    // stood 56 dp over a 104 dp faceplate, so the same 0.54 top-to-front
    // ratio is the target. Calibrated, not derived: the flat-projection
    // estimate (0.21 rad from the model's 86 mm height and 222 mm depth)
    // reads far too shallow, because a box this deep at the band's standoff
    // foreshortens its top hard. The old chrome's perspective, actually
    // built this time.
    static constexpr float  kDriveBandGazeDownRad = 0.420f;

    // The picture's actual on-screen height in this composition -- measured
    // through the full transform (band placement on the glass, sag, gaze
    // keystone), so the Ctrl+0 solve targets what the user really sees.
    static float    MeasurePictureHeightPx (const DeskSceneComposition & comp,
                                            const CurvedDisplaySurface & glass,
                                            int                          displayW,
                                            int                          displayH);

    // The camera's own right and up axes in WORLD space, read off the view
    // matrix's rotation. Both are unit length and square to the gaze, so a
    // quad spanned by them faces the camera head-on and all four of its
    // corners share one depth.
    static void     GetCameraBasis (const float view[16], float outRight[3], float outUp[3]);

    // How much world one screen pixel spans at `worldPt`'s depth, across and
    // down. Returns false for a point at or behind the eye plane. Read off
    // the projection the composition actually carries, so the user's zoom is
    // already in the answer rather than something a caller has to reapply.
    static bool     GetWorldPerPixel (const DeskSceneComposition & comp,
                                      const float                  worldPt[3],
                                      float                      & outPerPxX,
                                      float                      & outPerPxY);

    // The four world corners of one drive's name billboard, covering exactly
    // `labelPx` pixels starting `gapPx` below that drive's anchor. Corners
    // come back top-left, top-right, bottom-left, bottom-right. Returns false
    // when the anchor does not project.
    //
    // Pixels go IN and world corners come out, which is the inversion the
    // whole fix rests on: the name is specified in the units it has to be
    // legible in, and the scene is told where that lands.
    static bool     TryMakeDriveLabelQuad (const DeskSceneComposition & comp,
                                           int                          drive,
                                           const SIZE                 & labelPx,
                                           int                          gapPx,
                                           float                        outCorners[4][3]);

private:
    static void     SolveStandoff (const float             sceneMin[3],
                                   const float             sceneMax[3],
                                   float                   eyeY,
                                   float                   tanHalfY,
                                   float                   tanHalfX,
                                   float                 & outEyeZ);

    // Containment solved in the gaze's frame: returns the camera's DISTANCE
    // along the gaze (not a world Z), so the fit holds at any look-down.
    static void     SolveStandoffTilted (const float             sceneMin[3],
                                         const float             sceneMax[3],
                                         float                   aimY,
                                         float                   gazeDownRad,
                                         float                   tanHalfY,
                                         float                   tanHalfX,
                                         float                 & outDist);

    // How far back the fullscreen camera must stand for the WHOLE picture to
    // be on screen. Sampled over the picture band's boundary rather than
    // solved from its flat rect, because the band lies on a curved sheet:
    // the sag bulges each edge's middle toward the eye, and a nearer point
    // projects further off-axis than the corners it sits between.
    static float    SolvePictureStandoff (const DeskSceneMetrics & metrics,
                                          const float              monitorWorld[16],
                                          int                      displayW,
                                          int                      displayH,
                                          float                    aspect);

    // One full composition solve at a specific drive drop; Compute wraps it
    // with the gap-reserving correction.
    static HRESULT  SolveComposition (const RECT             & viewportPx,
                                      UINT                     dpi,
                                      int                      driveCount,
                                      const DeskSceneMetrics & metrics,
                                      float                    dropMm,
                                      DeskSceneComposition   & out,
                                      const DeskSceneView    & view = DeskSceneView {});
};
