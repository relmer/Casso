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
};


//
//  One frame's composition. World matrices carry the model->world axis remap
//  plus placement; viewProj is the single shared camera.
//
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
};


class DeskSceneLayout
{
public:
    // Deterministic for a given (viewport, dpi, driveCount, metrics).
    // Returns S_FALSE (composition zeroed) for an empty viewport, e.g.
    // minimized.
    static HRESULT  Compute (const RECT             & viewportPx,
                             UINT                     dpi,
                             int                      driveCount,
                             const DeskSceneMetrics & metrics,
                             DeskSceneComposition   & out);

    // The fixed model->world mount: X right stays, model Z (up) becomes world
    // Y, model Y (back) becomes world -Z, then translate by (tx, ty, tz).
    static void     MakeDeviceWorld (float tx, float ty, float tz, float out[16]);

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
                                            const DeskSceneMetrics & metrics);

    // The native-scale display height the 2D chrome established; sceneScale
    // is glass px height relative to this at the given DPI.
    static constexpr int    kScreenNativeHDp   = 384;

    // Shared camera: vertical fov, near/far planes (mm). The far plane
    // covers the containment solve's worst case -- an extremely narrow
    // viewport pushes the camera a long way back before the scene fits.
    static constexpr float  kFovY              = 0.35f;
    static constexpr float  kNearMm            = 10.0f;
    static constexpr float  kFarMm             = 30000.0f;

    // Composition: how far the drive row sits toward the viewer from the
    // monitor's front plane, the gap between two drives, and the breathing
    // margin the containment solve adds around the scene.
    static constexpr float  kDriveRowForwardMm = 170.0f;
    static constexpr float  kDriveGapMm        = 26.0f;
    static constexpr float  kContainMargin     = 1.02f;

    // Breathing room the Ctrl+0 shrink-wrap adds around the scene footprint.
    static constexpr int    kCenterPadPx       = 10;

    // Downward viewing angle: a person at a desk looks slightly down at the
    // hardware, which is also what reveals the top surfaces that make the
    // scene read as 3D instead of a flat front-on cutout.
    static constexpr float  kGazeDownRad       = 0.09f;

    // The picture's actual on-screen height in this composition -- measured
    // through the full transform (band placement on the glass, sag, gaze
    // keystone), so the Ctrl+0 solve targets what the user really sees.
    static float    MeasurePictureHeightPx (const DeskSceneComposition & comp,
                                            const CurvedDisplaySurface & glass,
                                            int                          displayW,
                                            int                          displayH);

private:
    static void     SolveStandoff (const float             sceneMin[3],
                                   const float             sceneMax[3],
                                   float                   eyeY,
                                   float                   tanHalfY,
                                   float                   tanHalfX,
                                   float                 & outEyeZ);
};
