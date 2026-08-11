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

void DeskSceneLayout::MakeDeviceWorld (float tx, float ty, float tz, float out[16])
{
    memset (out, 0, 16 * sizeof (float));
    out[0]  = 1.0f;
    out[6]  = -1.0f;
    out[9]  = 1.0f;
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
    MakeDeviceWorld (-monitorCx, 0.0f, 0.0f, out.monitorWorld);

    // Drives: a forward row on the ground plane -- toward the viewer, so
    // they read below the monitor from the straight-ahead camera. One drive
    // centers; two flank the centerline with a gap.
    driveTx[0] = (driveCount == 2) ? -(driveW + kDriveGapMm) * 0.5f : 0.0f;
    driveTx[1] = (driveW + kDriveGapMm) * 0.5f;

    for (int i = 0; i < driveCount; i++)
    {
        MakeDeviceWorld (driveTx[i] - driveCx, 0.0f, kDriveRowForwardMm, out.driveWorld[i]);
    }

    // Scene bounds in world space: the monitor spans its model box remapped
    // (y_world from model z, z_world from -model y), drives likewise shifted
    // forward.
    sceneMin[0] = metrics.monitorMin[0] - monitorCx;
    sceneMax[0] = metrics.monitorMax[0] - monitorCx;
    sceneMin[1] = metrics.monitorMin[2];
    sceneMax[1] = metrics.monitorMax[2];
    sceneMin[2] = -metrics.monitorMax[1];
    sceneMax[2] = -metrics.monitorMin[1];

    for (int i = 0; i < driveCount; i++)
    {
        float   lo[3] = { metrics.driveMin[0] - driveCx + driveTx[i],
                          metrics.driveMin[2],
                          kDriveRowForwardMm - metrics.driveMax[1] };
        float   hi[3] = { metrics.driveMax[0] - driveCx + driveTx[i],
                          metrics.driveMax[2],
                          kDriveRowForwardMm - metrics.driveMin[1] };

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
    SIZE   center = { displayWpx * 2, displayHpx * 3 };



    for (int pass = 0; pass < 5; pass++)
    {
        HRESULT               hr     = S_OK;
        DeskSceneComposition  comp;
        RECT                  vp     = { 0, 0, center.cx, center.cy };
        RECT                  fitted = {};
        int                   fh     = 0;
        float                 factor = 0.0f;

        hr = Compute (vp, dpi, driveCount, metrics, comp);

        if (hr != S_OK)
        {
            break;
        }

        fitted = ComputeAspectFitRectInRect (comp.glassRectPx, displayWpx, displayHpx);
        fh     = fitted.bottom - fitted.top;

        if (fh <= 0)
        {
            break;
        }

        if (std::abs (fh - displayHpx) <= 1)
        {
            break;
        }

        factor    = (float) displayHpx / (float) fh;
        center.cx = (LONG) lroundf ((float) center.cx * factor);
        center.cy = (LONG) lroundf ((float) center.cy * factor);
    }

    return center;
}
