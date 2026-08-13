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
//  DeskSceneLayout::SolveStandoffTilted
//
//  The same closed form, solved in the GAZE's frame rather than the world's,
//  so containment survives a steep look-down. The camera aims at (0, aimY, 0)
//  along f = (0, -sin g, -cos g) from distance `dist`, with up
//  u = (0, cos g, -sin g); for a point q relative to the aim point, its depth
//  is f.q + dist and its lateral offsets are u.q and qx, giving
//
//      dist >= |u.q| / tanHalfY - f.q       and       dist >= |qx| / tanHalfX - f.q
//
//  over all eight corners. Solving in world axes instead (SolveStandoff) is
//  exact only at zero gaze: the tilt rotates the near-bottom corners out of
//  the frustum the untilted solve just fitted, which is what cropped the
//  drive row's front edge.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneLayout::SolveStandoffTilted (const float   sceneMin[3],
                                           const float   sceneMax[3],
                                           float         aimY,
                                           float         gazeDownRad,
                                           float         tanHalfY,
                                           float         tanHalfX,
                                           float       & outDist)
{
    float   dist  = 0.0f;
    float   sinG  = std::sin (gazeDownRad);
    float   cosG  = std::cos (gazeDownRad);


    for (int corner = 0; corner < 8; corner++)
    {
        float   qx    = (corner & 1) ? sceneMax[0] : sceneMin[0];
        float   qy    = ((corner & 2) ? sceneMax[1] : sceneMin[1]) - aimY;
        float   qz    = (corner & 4) ? sceneMax[2] : sceneMin[2];
        float   depth = -sinG * qy - cosG * qz;               // f.q
        float   up    =  cosG * qy - sinG * qz;               // u.q

        dist = std::max (dist, std::abs (up) * kContainMargin / tanHalfY - depth);
        dist = std::max (dist, std::abs (qx) * kContainMargin / tanHalfX - depth);
    }

    outDist = dist;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::Compute
//
//  The drop correction: the input-mode row is fixed-height chrome living in
//  the projected gap between monitor and drives, and the gap scales with
//  the scene while the chrome does not -- so when the solve leaves too
//  little room, the drive drop deepens by the deficit (converted at the
//  glass's pixel density, a slight overestimate of the correction that
//  converges from above) and the composition re-solves.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneLayout::Compute (const RECT             & viewportPx,
                                  UINT                     dpi,
                                  int                      driveCount,
                                  const DeskSceneMetrics & metrics,
                                  DeskSceneComposition   & out,
                                  int                      reservedGapPx)
{
    HRESULT   hr     = S_OK;
    float     dropMm = kDriveDeskGapMm;



    hr = SolveComposition (viewportPx, dpi, driveCount, metrics, dropMm, out);

    for (int pass = 0; hr == S_OK && reservedGapPx > 0 && driveCount > 0 && pass < 3; pass++)
    {
        int     driveTop = INT_MAX;
        int     gapNow   = 0;
        float   pxPerMm  = 0.0f;
        int     glassHPx = out.glassRectPx.bottom - out.glassRectPx.top;

        for (int i = 0; i < driveCount; i++)
        {
            driveTop = std::min (driveTop, (int) out.driveRectPx[i].top);
        }

        gapNow  = driveTop - out.monitorRectPx.bottom;
        pxPerMm = (float) glassHPx / (metrics.glass.z1 - metrics.glass.z0);

        if (gapNow >= reservedGapPx || pxPerMm <= 0.0f)
        {
            break;
        }

        dropMm += (float) (reservedGapPx - gapNow) / pxPerMm;

        hr = SolveComposition (viewportPx, dpi, driveCount, metrics, dropMm, out);
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::SolveComposition
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneLayout::SolveComposition (const RECT             & viewportPx,
                                           UINT                     dpi,
                                           int                      driveCount,
                                           const DeskSceneMetrics & metrics,
                                           float                    dropMm,
                                           DeskSceneComposition   & out)
{
    HRESULT   hr           = S_OK;
    int       viewportW    = viewportPx.right - viewportPx.left;
    int       viewportH    = viewportPx.bottom - viewportPx.top;
    float     aspect       = 0.0f;
    float     tanHalfY     = std::tan (kFovY * 0.5f);
    float     tanHalfX     = 0.0f;
    float     monitorCx    = (metrics.monitorMin[0] + metrics.monitorMax[0]) * 0.5f;
    float     monitorW     = metrics.monitorMax[0] - metrics.monitorMin[0];
    float     driveW       = metrics.driveMax[0] - metrics.driveMin[0];
    float     driveCx      = (metrics.driveMin[0] + metrics.driveMax[0]) * 0.5f;
    float     glassCy      = 0.0f;
    float     eyeY         = 0.0f;
    float     eyeZ         = 0.0f;
    float     forwardMm    = 0.0f;
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
    // Drives at true size on the same desk, FLANKING the monitor at its own
    // depth -- where they actually stood. Standing them between the viewer
    // and the screen is the arrangement that looks wrong once the sizes are
    // real: a Disk II a foot nearer the eye than a 9-inch monitor projects
    // twice its apparent size, so the drives dominate the frame and the
    // picture shrinks to a stamp. Beside it, both are the same distance
    // away and read at their true relative size.
    //
    // `dropMm` is the side gap Compute widens to open room for the input
    // row -- sliding them further out along the desk.
    driveTx[0] = -((monitorW + driveW) * 0.5f + dropMm);
    driveTx[1] =  ((monitorW + driveW) * 0.5f + dropMm);
    forwardMm  = -metrics.monitorMin[1];   // fronts flush with the monitor's

    if (driveCount == 1)
    {
        driveTx[0] = driveTx[1];
    }

    for (int i = 0; i < driveCount; i++)
    {
        MakeDeviceWorld (driveTx[i] - driveCx, 0.0f, forwardMm, 1.0f, out.driveWorld[i]);
    }

    // Scene bounds in world space: the monitor spans its model box remapped
    // (y_world from model z, z_world from -model y), drives likewise scaled
    // and shifted forward.
    // Each device contributes its own body PLUS the ground clearance its
    // contact shadow needs (X and the toward-viewer Z only -- a shadow has no
    // height and the far side hides behind its own device).
    sceneMin[0] = metrics.monitorMin[0] - monitorCx - metrics.monitorPadSideMm;
    sceneMax[0] = metrics.monitorMax[0] - monitorCx + metrics.monitorPadSideMm;
    sceneMin[1] = metrics.monitorMin[2];
    sceneMax[1] = metrics.monitorMax[2];
    sceneMin[2] = -metrics.monitorMax[1];
    sceneMax[2] = -metrics.monitorMin[1] + metrics.monitorPadDepthMm;

    for (int i = 0; i < driveCount; i++)
    {
        float   lo[3] = { metrics.driveMin[0] - driveCx + driveTx[i] - metrics.drivePadSideMm,
                          metrics.driveMin[2],
                          forwardMm - metrics.driveMax[1] };
        float   hi[3] = { metrics.driveMax[0] - driveCx + driveTx[i] + metrics.drivePadSideMm,
                          metrics.driveMax[2],
                          forwardMm - metrics.driveMin[1] + metrics.drivePadDepthMm };

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

    // The eye is PLACED, not solved: a seated person kViewingDistanceMm from
    // the monitor's front plane, sitting kEyeAboveMonitorTopMm above the
    // monitor's top, looking at the middle of the screen. Every perspective
    // in the frame follows from that one position.
    {
        float   at[3]     = { 0.0f, glassCy, 0.0f };
        float   eyeUp     = metrics.monitorMax[2] + kEyeAboveMonitorTopMm;
        float   backOff   = 1.0f;
        float   fovY      = 0.0f;

        // Two passes at most: place the eye, solve the fov that shows the
        // whole scene from there, and if that fov would exceed what a person
        // can take in at once, LEAN BACK instead of widening further. Moving
        // the eye away along its own sight line is the physical answer to
        // "it does not all fit", and it keeps a pathological viewport from
        // turning the frame into a fisheye.
        for (int pass = 0; pass < 2; pass++)
        {
            float   eye[3]   = { 0.0f, glassCy + (eyeUp - glassCy) * backOff, kViewingDistanceMm * backOff };
            float   needTanX = 0.0f;
            float   needTanY = 0.0f;

            SceneCamera::LookAtRH (eye, at, out.view);

            // The only free variable is how much of that view the window
            // shows. Widening the fov never moves the camera, so the
            // geometry stays exact while the scene is fitted: for each scene
            // corner in view space (right-handed, looking down -Z), the
            // half-tangents it demands are |x| / depth and |y| / depth.
            for (int corner = 0; corner < 8; corner++)
            {
                float   pt[3]   = { (corner & 1) ? sceneMax[0] : sceneMin[0],
                                    (corner & 2) ? sceneMax[1] : sceneMin[1],
                                    (corner & 4) ? sceneMax[2] : sceneMin[2] };
                float   view[3] = {};
                float   depth   = 0.0f;

                if (!SceneCamera::TransformPoint (out.view, pt, view))
                {
                    continue;
                }

                depth = -view[2];

                // A corner at or behind the eye cannot constrain a fov; the
                // near clip owns that case.
                if (depth <= kNearMm)
                {
                    continue;
                }

                needTanX = std::max (needTanX, std::abs (view[0]) / depth);
                needTanY = std::max (needTanY, std::abs (view[1]) / depth);
            }

            // Fit the tighter axis: a wide window is bounded by height, a
            // tall one by width.
            tanHalfY = std::max (needTanY, needTanX / aspect) * kContainMargin;
            tanHalfX = tanHalfY * aspect;

            eyeY = eye[1];
            eyeZ = eye[2];

            if (tanHalfY <= std::tan (kMaxFovY * 0.5f) || pass > 0)
            {
                break;
            }

            backOff *= tanHalfY / std::tan (kMaxFovY * 0.5f);
        }

        fovY = std::clamp (2.0f * std::atan (tanHalfY), kMinFovY, kMaxFovY);

        SceneCamera::PerspectiveFovRH (fovY, aspect, kNearMm, kFarMm, out.proj);
        SceneCamera::Mul44            (out.view, out.proj, out.viewProj);
    }

    {

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
//  Drives only, full model scale, centered row on the ground plane. The row
//  is symmetric about its center, so the closed-form standoff with the aim at
//  the row's vertical center contains it without the desk composition's
//  asymmetric refine passes -- solved in the gaze's frame, so a steep
//  look-down contains as exactly as a shallow one.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneLayout::ComputeStrip (const RECT             & viewportPx,
                                       UINT                     dpi,
                                       int                      driveCount,
                                       const DeskSceneMetrics & metrics,
                                       DeskSceneComposition   & out,
                                       float                    gazeDownRad)
{
    HRESULT   hr          = S_OK;
    int       viewportW   = viewportPx.right - viewportPx.left;
    int       viewportH   = viewportPx.bottom - viewportPx.top;
    float     aspect      = 0.0f;
    float     tanHalfY    = std::tan (kFovY * 0.5f);
    float     driveW      = metrics.driveMax[0] - metrics.driveMin[0];
    float     driveCx     = (metrics.driveMin[0] + metrics.driveMax[0]) * 0.5f;
    float     rowCy       = 0.0f;
    float     dist        = 0.0f;
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
        float   lo[3] = { metrics.driveMin[0] - driveCx + driveTx[i] - metrics.drivePadSideMm,
                          metrics.driveMin[2],
                          -metrics.driveMax[1] };
        float   hi[3] = { metrics.driveMax[0] - driveCx + driveTx[i] + metrics.drivePadSideMm,
                          metrics.driveMax[2],
                          -metrics.driveMin[1] + metrics.drivePadDepthMm };

        MakeDeviceWorld (driveTx[i] - driveCx, 0.0f, 0.0f, 1.0f, out.driveWorld[i]);

        for (int axis = 0; axis < 3; axis++)
        {
            sceneMin[axis] = std::min (sceneMin[axis], lo[axis]);
            sceneMax[axis] = std::max (sceneMax[axis], hi[axis]);
        }
    }

    rowCy = (sceneMin[1] + sceneMax[1]) * 0.5f;

    SolveStandoffTilted (sceneMin, sceneMax, rowCy, gazeDownRad, tanHalfY, tanHalfY * aspect, dist);

    {
        float   eye[3] = { 0.0f, rowCy + std::sin (gazeDownRad) * dist, std::cos (gazeDownRad) * dist };
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
                                              const DeskSceneMetrics & metrics,
                                              int                      reservedGapPx)
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

        hr = Compute (vp, dpi, driveCount, metrics, comp, reservedGapPx);

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
