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

    // The camera looks straight ahead at glass-center height: the glass
    // stays front-facing for the display, and everything placed lower or
    // off-center picks up its position's parallax automatically.
    glassCy = (metrics.glass.z0 + metrics.glass.z1) * 0.5f;
    eyeY    = glassCy;

    SolveStandoff (sceneMin, sceneMax, eyeY, tanHalfY, tanHalfX, eyeZ);

    {
        float   eye[3] = { 0.0f, eyeY, eyeZ };
        float   at[3]  = { 0.0f, eyeY, 0.0f };

        SceneCamera::LookAtRH         (eye, at, out.view);
        SceneCamera::PerspectiveFovRH (kFovY, aspect, kNearMm, kFarMm, out.proj);
        SceneCamera::Mul44            (out.view, out.proj, out.viewProj);
    }

    // Scene scale and the projected glass rect: the glass's on-screen
    // bounds against the 2D chrome's native 384 dp. The glass corners
    // project at the monitor's front plane depth (its widest silhouette --
    // the sag only recedes from there), centered on the camera axis after
    // the monitor's own centering translation.
    {
        float   glassZ  = -metrics.glass.baseY;
        float   tl[3]   = { metrics.glass.x0 - monitorCx, metrics.glass.z1, glassZ };
        float   br[3]   = { metrics.glass.x1 - monitorCx, metrics.glass.z0, glassZ };
        float   tlPx[2] = {};
        float   brPx[2] = {};

        if (SceneCamera::ProjectToScreen (out.viewProj, tl, viewportPx, tlPx) &&
            SceneCamera::ProjectToScreen (out.viewProj, br, viewportPx, brPx))
        {
            float   nativeHPx = (float) kScreenNativeHDp * (float) dpi / 96.0f;

            out.sceneScale = (brPx[1] - tlPx[1]) / nativeHPx;

            out.glassRectPx.left   = (LONG) std::floor (tlPx[0]);
            out.glassRectPx.top    = (LONG) std::floor (tlPx[1]);
            out.glassRectPx.right  = (LONG) std::ceil (brPx[0]);
            out.glassRectPx.bottom = (LONG) std::ceil (brPx[1]);
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
