#include "Pch.h"

#include "Ui/Scene/DeskSceneLayout.h"

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

    BAIL_OUT_IF (viewportW <= 0 || viewportH <= 0, S_FALSE);

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

    // Scene scale: the glass's on-screen height against the 2D chrome's
    // native 384 dp. The glass top/bottom project at the monitor's front
    // plane depth.
    {
        float   glassZ    = -metrics.glass.baseY;
        float   top[3]    = { 0.0f, metrics.glass.z1, glassZ };
        float   bottom[3] = { 0.0f, metrics.glass.z0, glassZ };
        float   topPx[2]  = {};
        float   botPx[2]  = {};

        if (SceneCamera::ProjectToScreen (out.viewProj, top, viewportPx, topPx) &&
            SceneCamera::ProjectToScreen (out.viewProj, bottom, viewportPx, botPx))
        {
            float   nativeHPx = (float) kScreenNativeHDp * (float) dpi / 96.0f;

            out.sceneScale = (botPx[1] - topPx[1]) / nativeHPx;
        }
    }

Error:
    return hr;
}
