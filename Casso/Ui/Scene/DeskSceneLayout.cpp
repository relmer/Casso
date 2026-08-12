#include "Pch.h"

#include "Ui/Scene/DeskSceneLayout.h"

#include "CrtPostProcess.h"
#include "Render/SceneCamera.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::MakeDeviceWorld
//
//  Row-vector convention (world = model * M): row 0 is the image of the model
//  x axis, row 1 of the y axis (back -> world -Z), row 2 of the z axis
//  (up -> world Y), row 3 the translation.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneLayout::MakeDeviceWorld (float tx, float ty, float tz, float scale, float out[16])
{
    memset (out, 0, 16 * sizeof (float));
    out[0]  = scale;
    out[6]  = -scale;
    out[9]  = scale;
    out[12] = tx;
    out[13] = ty;
    out[14] = tz;
    out[15] = 1.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::SolveStandoff
//
//  Closed-form containment: for a camera at (0, eyeY, eyeZ) looking straight
//  down -Z, a world point (x, y, z) is inside the frustum when its lateral
//  offsets fit the fov cone at its depth:
//
//      |y - eyeY| <= tanHalfY * (eyeZ - z)
//      |x|        <= tanHalfX * (eyeZ - z)
//
//  Each corner therefore demands eyeZ >= z + offset / tan; the standoff is
//  the maximum demand over all eight corners of the scene bounds, margin
//  applied to the offsets so the scene never kisses the viewport edge.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneLayout::SolveStandoff (const float   sceneMin[3],
                                     const float   sceneMax[3],
                                     float         eyeY,
                                     float         tanHalfY,
                                     float         tanHalfX,
                                     float       & outEyeZ)
{
    float   eyeZ = 0.0f;



    for (int corner = 0; corner < 8; corner++)
    {
        float   x = (corner & 1) ? sceneMax[0] : sceneMin[0];
        float   y = (corner & 2) ? sceneMax[1] : sceneMin[1];
        float   z = (corner & 4) ? sceneMax[2] : sceneMin[2];

        eyeZ = std::max (eyeZ, z + std::abs (y - eyeY) * kContainMargin / tanHalfY);
        eyeZ = std::max (eyeZ, z + std::abs (x) * kContainMargin / tanHalfX);
    }

    outEyeZ = eyeZ;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::Compute
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneLayout::Compute (const RECT             & viewportPx,
                                  UINT                     dpi,
                                  int                      driveCount,
                                  const DeskSceneMetrics & metrics,
                                  DeskSceneComposition   & out)
{
    HRESULT   hr           = S_OK;
    int       viewportW    = viewportPx.right - viewportPx.left;
    int       viewportH    = viewportPx.bottom - viewportPx.top;
    float     aspect       = 0.0f;
    float     tanHalfY     = std::tan (kFovY * 0.5f);
    float     tanHalfX     = 0.0f;
    float     monitorCx    = (metrics.monitorMin[0] + metrics.monitorMax[0]) * 0.5f;
    float     driveW       = metrics.driveMax[0] - metrics.driveMin[0];
    float     driveCx      = (metrics.driveMin[0] + metrics.driveMax[0]) * 0.5f;
    float     glassCy      = 0.0f;
    float     eyeY         = 0.0f;
    float     eyeZ         = 0.0f;
    float     sceneMin[3]  = {};
    float     sceneMax[3]  = {};
    float     driveTx[2]   = {};



    out = {};

    CBRAEx (driveCount >= 0 && driveCount <= 2, E_INVALIDARG);
    CBRAEx (dpi > 0, E_INVALIDARG);

    BAIL_OUT_IF (viewportW <= 0 || viewportH <= 0, S_FALSE);   // EHM-ALLOW-SFALSE: minimized/zero viewport is a routine skip-this-frame state the caller tests for, not an error

    out.viewportPx = viewportPx;
    out.driveCount = driveCount;
    aspect         = (float) viewportW / (float) viewportH;
    tanHalfX       = tanHalfY * aspect;

    // Monitor: centered on x = 0, feet on the ground plane, front at z = 0.
    MakeDeviceWorld (-monitorCx, 0.0f, 0.0f, 1.0f, out.monitorWorld);

    // Drives: a forward row on the ground plane -- toward the viewer, so
    // they read below the monitor from the straight-ahead camera -- at the
    // band's placement scale. One drive centers; two flank the centerline
    // with a gap.
    driveTx[0] = (driveCount == 2) ? -(driveW * kDriveScale + kDriveGapMm) * 0.5f : 0.0f;
    driveTx[1] = (driveW * kDriveScale + kDriveGapMm) * 0.5f;

    for (int i = 0; i < driveCount; i++)
    {
        MakeDeviceWorld (driveTx[i] - driveCx * kDriveScale, -kDriveDropMm, kDriveRowForwardMm,
                         kDriveScale, out.driveWorld[i]);
    }

    // Scene bounds in world space: the monitor spans its model box remapped
    // (y_world from model z, z_world from -model y), drives likewise scaled
    // and shifted forward.
    sceneMin[0] = metrics.monitorMin[0] - monitorCx;
    sceneMax[0] = metrics.monitorMax[0] - monitorCx;
    sceneMin[1] = metrics.monitorMin[2];
    sceneMax[1] = metrics.monitorMax[2];
    sceneMin[2] = -metrics.monitorMax[1];
    sceneMax[2] = -metrics.monitorMin[1];

    for (int i = 0; i < driveCount; i++)
    {
        float   lo[3] = { (metrics.driveMin[0] - driveCx) * kDriveScale + driveTx[i],
                          metrics.driveMin[2] * kDriveScale - kDriveDropMm,
                          kDriveRowForwardMm - metrics.driveMax[1] * kDriveScale };
        float   hi[3] = { (metrics.driveMax[0] - driveCx) * kDriveScale + driveTx[i],
                          metrics.driveMax[2] * kDriveScale - kDriveDropMm,
                          kDriveRowForwardMm - metrics.driveMin[1] * kDriveScale };

        for (int axis = 0; axis < 3; axis++)
        {
            sceneMin[axis] = std::min (sceneMin[axis], lo[axis]);
            sceneMax[axis] = std::max (sceneMax[axis], hi[axis]);
        }
    }

    // The camera looks at the glass center from slightly above (a person at
    // a desk), so top surfaces show and every device picks up its position's
    // parallax automatically. The straight-axis closed form seeds the
    // standoff; a few refine passes then project the actual scene corners
    // and tighten distance AND vertical aim until the scene fills the
    // viewport with just the contain margin -- without this the symmetric
    // frustum wastes the whole top margin on a bottom-heavy scene.
    glassCy = (metrics.glass.z0 + metrics.glass.z1) * 0.5f;
    eyeY    = glassCy;

    SolveStandoff (sceneMin, sceneMax, eyeY, tanHalfY, tanHalfX, eyeZ);

    {
        float   atY      = glassCy;
        float   standoff = eyeZ;
        float   target   = 1.0f / kContainMargin;

        for (int pass = 0; pass < 4; pass++)
        {
            float   eye[3]     = { 0.0f, atY + std::sin (kGazeDownRad) * standoff, std::cos (kGazeDownRad) * standoff };
            float   at[3]      = { 0.0f, atY, 0.0f };
            float   ndcMin[2]  = { FLT_MAX, FLT_MAX };
            float   ndcMax[2]  = { -FLT_MAX, -FLT_MAX };
            bool    allVisible = true;

            SceneCamera::LookAtRH         (eye, at, out.view);
            SceneCamera::PerspectiveFovRH (kFovY, aspect, kNearMm, kFarMm, out.proj);
            SceneCamera::Mul44            (out.view, out.proj, out.viewProj);

            for (int corner = 0; corner < 8; corner++)
            {
                float   pt[3]  = { (corner & 1) ? sceneMax[0] : sceneMin[0],
                                   (corner & 2) ? sceneMax[1] : sceneMin[1],
                                   (corner & 4) ? sceneMax[2] : sceneMin[2] };
                float   ndc[3] = {};

                if (!SceneCamera::TransformPoint (out.viewProj, pt, ndc))
                {
                    allVisible = false;
                    break;
                }

                ndcMin[0] = std::min (ndcMin[0], ndc[0]);  ndcMax[0] = std::max (ndcMax[0], ndc[0]);
                ndcMin[1] = std::min (ndcMin[1], ndc[1]);  ndcMax[1] = std::max (ndcMax[1], ndc[1]);
            }

            if (!allVisible)
            {
                standoff *= 1.5f;
                continue;
            }

            {
                float   extent = std::max ({ std::abs (ndcMin[0]), std::abs (ndcMax[0]),
                                             (ndcMax[1] - ndcMin[1]) * 0.5f });
                float   center = (ndcMin[1] + ndcMax[1]) * 0.5f;

                standoff *= std::max (extent / target, 0.05f);
                atY      += center * tanHalfY * standoff;
            }
        }

        // Rebuild the camera from the final corrected standoff/aim -- the
        // loop's matrices are one correction behind.
        {
            float   eye[3] = { 0.0f, atY + std::sin (kGazeDownRad) * standoff, std::cos (kGazeDownRad) * standoff };
            float   at[3]  = { 0.0f, atY, 0.0f };

            SceneCamera::LookAtRH         (eye, at, out.view);
            SceneCamera::PerspectiveFovRH (kFovY, aspect, kNearMm, kFarMm, out.proj);
            SceneCamera::Mul44            (out.view, out.proj, out.viewProj);
        }

        // The scene's own projected footprint, for aspect-matched sizing
        // (Ctrl+0 shrink-wraps the window around it).
        {
            float   pxMin[2] = { FLT_MAX, FLT_MAX };
            float   pxMax[2] = { -FLT_MAX, -FLT_MAX };
            bool    all      = true;

            for (int corner = 0; corner < 8; corner++)
            {
                float   pt[3] = { (corner & 1) ? sceneMax[0] : sceneMin[0],
                                  (corner & 2) ? sceneMax[1] : sceneMin[1],
                                  (corner & 4) ? sceneMax[2] : sceneMin[2] };
                float   px[2] = {};

                if (!SceneCamera::ProjectToScreen (out.viewProj, pt, viewportPx, px))
                {
                    all = false;
                    break;
                }

                pxMin[0] = std::min (pxMin[0], px[0]);  pxMax[0] = std::max (pxMax[0], px[0]);
                pxMin[1] = std::min (pxMin[1], px[1]);  pxMax[1] = std::max (pxMax[1], px[1]);
            }

            if (all)
            {
                out.sceneRectPx.left   = (LONG) std::floor (pxMin[0]);
                out.sceneRectPx.top    = (LONG) std::floor (pxMin[1]);
                out.sceneRectPx.right  = (LONG) std::ceil (pxMax[0]);
                out.sceneRectPx.bottom = (LONG) std::ceil (pxMax[1]);
            }
        }
    }

    // Projected per-device bounds: each device's model box through its own
    // world matrix. The drive rects are the tooltip anchors and drag-drop
    // targets (the 2D widgets' OuterRects); the monitor rect is what the
    // in-scene chrome (the input-mode button row) lays out against.
    for (int i = -1; i < driveCount; i++)
    {
        const float  * world    = (i < 0) ? out.monitorWorld : out.driveWorld[i];
        const float  * boxMin   = (i < 0) ? metrics.monitorMin : metrics.driveMin;
        const float  * boxMax   = (i < 0) ? metrics.monitorMax : metrics.driveMax;
        RECT         & outPx    = (i < 0) ? out.monitorRectPx : out.driveRectPx[i];
        float          pxMin[2] = { FLT_MAX, FLT_MAX };
        float          pxMax[2] = { -FLT_MAX, -FLT_MAX };
        bool           all      = true;

        for (int corner = 0; corner < 8; corner++)
        {
            float   pt[3]     = { (corner & 1) ? boxMax[0] : boxMin[0],
                                  (corner & 2) ? boxMax[1] : boxMin[1],
                                  (corner & 4) ? boxMax[2] : boxMin[2] };
            float   worldPt[3] = {};
            float   px[2]      = {};

            if (!SceneCamera::TransformPoint (world, pt, worldPt) ||
                !SceneCamera::ProjectToScreen (out.viewProj, worldPt, viewportPx, px))
            {
                all = false;
                break;
            }

            pxMin[0] = std::min (pxMin[0], px[0]);  pxMax[0] = std::max (pxMax[0], px[0]);
            pxMin[1] = std::min (pxMin[1], px[1]);  pxMax[1] = std::max (pxMax[1], px[1]);
        }

        if (all)
        {
            outPx.left   = (LONG) std::floor (pxMin[0]);
            outPx.top    = (LONG) std::floor (pxMin[1]);
            outPx.right  = (LONG) std::ceil (pxMax[0]);
            outPx.bottom = (LONG) std::ceil (pxMax[1]);
        }
    }

    // Scene scale and the projected glass rect: the glass's on-screen
    // bounds against the 2D chrome's native 384 dp. All four glass corners
    // project (the downward gaze keystones the quad slightly), and the rect
    // is their bounding box at the monitor's front plane depth.
    {
        float   glassZ    = -metrics.glass.baseY;
        float   pxMin[2]  = { FLT_MAX, FLT_MAX };
        float   pxMax[2]  = { -FLT_MAX, -FLT_MAX };
        bool    projected = true;

        for (int corner = 0; corner < 4; corner++)
        {
            float   pt[3] = { (corner & 1) ? metrics.glass.x1 - monitorCx : metrics.glass.x0 - monitorCx,
                              (corner & 2) ? metrics.glass.z1 : metrics.glass.z0,
                              glassZ };
            float   px[2] = {};

            if (!SceneCamera::ProjectToScreen (out.viewProj, pt, viewportPx, px))
            {
                projected = false;
                break;
            }

            pxMin[0] = std::min (pxMin[0], px[0]);  pxMax[0] = std::max (pxMax[0], px[0]);
            pxMin[1] = std::min (pxMin[1], px[1]);  pxMax[1] = std::max (pxMax[1], px[1]);
        }

        if (projected)
        {
            float   nativeHPx = (float) kScreenNativeHDp * (float) dpi / 96.0f;

            out.sceneScale = (pxMax[1] - pxMin[1]) / nativeHPx;

            out.glassRectPx.left   = (LONG) std::floor (pxMin[0]);
            out.glassRectPx.top    = (LONG) std::floor (pxMin[1]);
            out.glassRectPx.right  = (LONG) std::ceil (pxMax[0]);
            out.glassRectPx.bottom = (LONG) std::ceil (pxMax[1]);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::ComputeGlassFill
//
//  The fullscreen camera: the monitor mounts exactly as in the windowed
//  composition (so the glass surface, hit tester, and picture band all keep
//  working unchanged), but the camera solves to FILL the viewport with the
//  glass rect -- the shorter-standoff axis binds and the other crops. The
//  projected glass rect IS the viewport, which is what routes the CRT target
//  to the whole screen.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneLayout::ComputeGlassFill (const RECT             & viewportPx,
                                           UINT                     dpi,
                                           const DeskSceneMetrics & metrics,
                                           DeskSceneComposition   & out)
{
    HRESULT   hr        = S_OK;
    int       viewportW = viewportPx.right - viewportPx.left;
    int       viewportH = viewportPx.bottom - viewportPx.top;
    float     monitorCx = (metrics.monitorMin[0] + metrics.monitorMax[0]) * 0.5f;
    float     glassCx   = (metrics.glass.x0 + metrics.glass.x1) * 0.5f - monitorCx;
    float     glassCy   = (metrics.glass.z0 + metrics.glass.z1) * 0.5f;
    float     glassW    = metrics.glass.x1 - metrics.glass.x0;
    float     glassH    = metrics.glass.z1 - metrics.glass.z0;
    float     nativeHPx = 0.0f;



    out = {};

    CBRAEx (dpi > 0, E_INVALIDARG);

    BAIL_OUT_IF (viewportW <= 0 || viewportH <= 0, S_FALSE);   // EHM-ALLOW-SFALSE: minimized/zero viewport is a routine skip-this-frame state the caller tests for, not an error

    out.viewportPx = viewportPx;
    out.driveCount = 0;

    MakeDeviceWorld (-monitorCx, 0.0f, 0.0f, 1.0f, out.monitorWorld);

    SceneCamera::SolveGlassFillCamera (glassCx, glassCy, glassW, glassH,
                                       -metrics.glass.baseY,
                                       kFovY, (float) viewportW / (float) viewportH,
                                       kNearMm, kFarMm,
                                       out.view, out.proj, out.viewProj);

    // The glass claims the whole viewport; scene scale keys off the SCREEN
    // height so the CRT target is sized for what is actually visible.
    out.glassRectPx = viewportPx;
    out.sceneRectPx = viewportPx;

    nativeHPx      = (float) kScreenNativeHDp * (float) dpi / 96.0f;
    out.sceneScale = (float) viewportH / nativeHPx;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::ComputeStrip
//
//  Drives only, full model scale, centered row on the ground plane. The
//  strip band is short and the row symmetric about its center, so the
//  closed-form standoff with the aim at the row's vertical center contains
//  it without the desk composition's asymmetric refine passes.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneLayout::ComputeStrip (const RECT             & viewportPx,
                                       UINT                     dpi,
                                       int                      driveCount,
                                       const DeskSceneMetrics & metrics,
                                       DeskSceneComposition   & out)
{
    HRESULT   hr          = S_OK;
    int       viewportW   = viewportPx.right - viewportPx.left;
    int       viewportH   = viewportPx.bottom - viewportPx.top;
    float     aspect      = 0.0f;
    float     tanHalfY    = std::tan (kFovY * 0.5f);
    float     driveW      = metrics.driveMax[0] - metrics.driveMin[0];
    float     driveCx     = (metrics.driveMin[0] + metrics.driveMax[0]) * 0.5f;
    float     rowCy       = 0.0f;
    float     eyeZ        = 0.0f;
    float     sceneMin[3] = { FLT_MAX, FLT_MAX, FLT_MAX };
    float     sceneMax[3] = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
    float     driveTx[2]  = {};



    out = {};

    CBRAEx (driveCount >= 1 && driveCount <= 2, E_INVALIDARG);
    CBRAEx (dpi > 0, E_INVALIDARG);

    BAIL_OUT_IF (viewportW <= 0 || viewportH <= 0, S_FALSE);   // EHM-ALLOW-SFALSE: minimized/zero viewport is a routine skip-this-frame state the caller tests for, not an error

    out.viewportPx = viewportPx;
    out.driveCount = driveCount;
    aspect         = (float) viewportW / (float) viewportH;

    driveTx[0] = (driveCount == 2) ? -(driveW + kDriveGapMm) * 0.5f : 0.0f;
    driveTx[1] = (driveW + kDriveGapMm) * 0.5f;

    for (int i = 0; i < driveCount; i++)
    {
        float   lo[3] = { metrics.driveMin[0] - driveCx + driveTx[i],
                          metrics.driveMin[2],
                          -metrics.driveMax[1] };
        float   hi[3] = { metrics.driveMax[0] - driveCx + driveTx[i],
                          metrics.driveMax[2],
                          -metrics.driveMin[1] };

        MakeDeviceWorld (driveTx[i] - driveCx, 0.0f, 0.0f, 1.0f, out.driveWorld[i]);

        for (int axis = 0; axis < 3; axis++)
        {
            sceneMin[axis] = std::min (sceneMin[axis], lo[axis]);
            sceneMax[axis] = std::max (sceneMax[axis], hi[axis]);
        }
    }

    rowCy = (sceneMin[1] + sceneMax[1]) * 0.5f;

    SolveStandoff (sceneMin, sceneMax, rowCy, tanHalfY, tanHalfY * aspect, eyeZ);

    {
        float   eye[3] = { 0.0f, rowCy + std::sin (kGazeDownRad) * eyeZ, std::cos (kGazeDownRad) * eyeZ };
        float   at[3]  = { 0.0f, rowCy, 0.0f };

        SceneCamera::LookAtRH         (eye, at, out.view);
        SceneCamera::PerspectiveFovRH (kFovY, aspect, kNearMm, kFarMm, out.proj);
        SceneCamera::Mul44            (out.view, out.proj, out.viewProj);
    }

    // Projected per-drive bounds: tooltip anchors and hit rects, exactly as
    // the desk composition provides them.
    for (int i = 0; i < driveCount; i++)
    {
        float   pxMin[2] = { FLT_MAX, FLT_MAX };
        float   pxMax[2] = { -FLT_MAX, -FLT_MAX };
        bool    all      = true;

        for (int corner = 0; corner < 8; corner++)
        {
            float   pt[3]    = { (corner & 1) ? metrics.driveMax[0] : metrics.driveMin[0],
                                 (corner & 2) ? metrics.driveMax[1] : metrics.driveMin[1],
                                 (corner & 4) ? metrics.driveMax[2] : metrics.driveMin[2] };
            float   world[3] = {};
            float   px[2]    = {};

            if (!SceneCamera::TransformPoint (out.driveWorld[i], pt, world) ||
                !SceneCamera::ProjectToScreen (out.viewProj, world, viewportPx, px))
            {
                all = false;
                break;
            }

            pxMin[0] = std::min (pxMin[0], px[0]);  pxMax[0] = std::max (pxMax[0], px[0]);
            pxMin[1] = std::min (pxMin[1], px[1]);  pxMax[1] = std::max (pxMax[1], px[1]);
        }

        if (all)
        {
            out.driveRectPx[i].left   = (LONG) std::floor (pxMin[0]);
            out.driveRectPx[i].top    = (LONG) std::floor (pxMin[1]);
            out.driveRectPx[i].right  = (LONG) std::ceil (pxMax[0]);
            out.driveRectPx[i].bottom = (LONG) std::ceil (pxMax[1]);
        }
    }

    out.sceneRectPx = viewportPx;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::MeasurePictureHeightPx
//
//  Projects the picture band's top and bottom (at horizontal center) through
//  the glass surface and the composition's camera -- the height includes the
//  sag's forward bulge and the gaze keystone, unlike any texture-space
//  measure.
//
////////////////////////////////////////////////////////////////////////////////

float DeskSceneLayout::MeasurePictureHeightPx (const DeskSceneComposition & comp,
                                               const CurvedDisplaySurface & glass,
                                               int                          displayW,
                                               int                          displayH)
{
    float  bandU0     = 0.0f;
    float  bandV0     = 0.0f;
    float  bandU1     = 1.0f;
    float  bandV1     = 1.0f;
    float  uMid       = 0.0f;
    float  topPx[2]   = {};
    float  botPx[2]   = {};
    float  modelPt[3] = {};
    float  worldPt[3] = {};



    CurvedDisplayMath::ComputePictureBand (glass, displayW, displayH, bandU0, bandV0, bandU1, bandV1);

    uMid = (bandU0 + bandU1) * 0.5f;

    CurvedDisplayMath::ModelPointFromUv (glass, uMid, bandV0, modelPt);

    if (!SceneCamera::TransformPoint (comp.monitorWorld, modelPt, worldPt) ||
        !SceneCamera::ProjectToScreen (comp.viewProj, worldPt, comp.viewportPx, topPx))
    {
        return 0.0f;
    }

    CurvedDisplayMath::ModelPointFromUv (glass, uMid, bandV1, modelPt);

    if (!SceneCamera::TransformPoint (comp.monitorWorld, modelPt, worldPt) ||
        !SceneCamera::ProjectToScreen (comp.viewProj, worldPt, comp.viewportPx, botPx))
    {
        return 0.0f;
    }

    return botPx[1] - topPx[1];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::CenterSizeForDisplayPx
//
//  The picture aspect-fits inside the projected glass (whose own aspect the
//  model fixes), so the solve measures the FITTED height each pass and scales
//  the trial center uniformly by the shortfall. glassPx(center) is piecewise
//  linear in the center size (the containment standoff scales with whichever
//  axis binds), so a shared factor converges in a couple of passes. Seeded
//  generously above the answer so the first Compute lands in the same linear
//  piece.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DeskSceneLayout::CenterSizeForDisplayPx (int                      displayWpx,
                                              int                      displayHpx,
                                              UINT                     dpi,
                                              int                      driveCount,
                                              const DeskSceneMetrics & metrics)
{
    SIZE   center  = { displayWpx * 2, displayHpx * 3 };
    bool   wrapped = false;



    for (int pass = 0; pass < 7; pass++)
    {
        HRESULT               hr     = S_OK;
        DeskSceneComposition  comp;
        RECT                  vp     = { 0, 0, center.cx, center.cy };
        int                   fh     = 0;
        float                 factor = 0.0f;

        hr = Compute (vp, dpi, driveCount, metrics, comp);

        if (hr != S_OK)
        {
            break;
        }

        fh = (int) lroundf (MeasurePictureHeightPx (comp, metrics.glass, displayWpx, displayHpx));

        if (fh <= 0)
        {
            break;
        }

        // Converged: the CURRENT center produces the target picture height
        // (never return a further-updated center the loop has not verified).
        if (std::abs (fh - displayHpx) <= 1)
        {
            break;
        }

        // First correction: shrink-wrap the center around the scene's
        // projected footprint (plus a small pad) so a viewport-aspect
        // mismatch never letterboxes slack above and below the monitor.
        // After that the aspect is settled and pure uniform scaling
        // converges on the linear glassPx(center) piece -- re-wrapping every
        // pass oscillates, because the forward drive row's projection is
        // strongly standoff-dependent.
        factor = (float) displayHpx / (float) fh;

        if (!wrapped)
        {
            center.cx = (LONG) lroundf ((float) (comp.sceneRectPx.right - comp.sceneRectPx.left) * factor) + kCenterPadPx * 2;
            center.cy = (LONG) lroundf ((float) (comp.sceneRectPx.bottom - comp.sceneRectPx.top) * factor) + kCenterPadPx * 2;
            wrapped   = true;
        }
        else
        {
            center.cx = (LONG) lroundf ((float) center.cx * factor);
            center.cy = (LONG) lroundf ((float) center.cy * factor);
        }
    }

    return center;
}
