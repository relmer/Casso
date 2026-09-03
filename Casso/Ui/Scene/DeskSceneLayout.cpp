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
//  `reservedGapPx` is inert now and kept only so callers need not change in
//  lockstep. It used to widen a gap between the monitor and the drive row for
//  the input-mode chrome to sit in, by sliding the drives forward and
//  re-solving. There is no such gap to widen any more: the monitor STANDS ON
//  the drives and every front face shares one plane, so the two are adjacent
//  by construction. The input row needs a home of its own rather than a seam
//  to hide in.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneLayout::Compute (const RECT             & viewportPx,
                                  UINT                     dpi,
                                  int                      driveCount,
                                  const DeskSceneMetrics & metrics,
                                  DeskSceneComposition   & out,
                                  int                      reservedGapPx,
                                  const DeskSceneView    & view)
{
    HRESULT   hr = S_OK;



    hr = SolveComposition (viewportPx, dpi, driveCount, metrics, kDriveDeskGapMm, out, view);

    (void) reservedGapPx;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::ApplyViewTransform
//
//  Post-multiplies the projection by a clip-space zoom and shift:
//
//      x' = zoom * x + panX * w
//      y' = zoom * y + panY * w
//
//  Multiplying the pan by w is what makes it a CAMERA shift rather than a
//  skew -- the offset lands after the perspective divide, so near and far
//  geometry move together instead of shearing apart with depth.
//
//  z is untouched on purpose. Depth has to keep meaning what it meant or the
//  shadow maps, which are built against the same world, stop lining up with
//  what the camera sees.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneLayout::ApplyViewTransform (const DeskSceneView & view, float proj[16])
{
    int  row = 0;



    if (view.IsIdentity())
    {
        return;
    }

    for (row = 0; row < 4; row++)
    {
        float  x = proj[row * 4 + 0];
        float  y = proj[row * 4 + 1];
        float  w = proj[row * 4 + 3];

        proj[row * 4 + 0] = x * view.zoom + w * view.panX;
        proj[row * 4 + 1] = y * view.zoom + w * view.panY;
    }
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
                                           DeskSceneComposition   & out,
                                           const DeskSceneView    & view)
{
    HRESULT  hr            = S_OK;
    int      viewportW     = viewportPx.right - viewportPx.left;
    int      viewportH     = viewportPx.bottom - viewportPx.top;
    float    aspect        = 0.0f;
    float    tanHalfY      = std::tan (kFovY * 0.5f);
    float    tanHalfX      = 0.0f;
    float    monitorCx     = (metrics.monitorMin[0] + metrics.monitorMax[0]) * 0.5f;
    float    monitorW      = metrics.monitorMax[0] - metrics.monitorMin[0];
    float    driveW        = metrics.driveMax[0] - metrics.driveMin[0];
    float    driveCx       = (metrics.driveMin[0] + metrics.driveMax[0]) * 0.5f;
    float    glassCy       = 0.0f;
    float    eyeY          = 0.0f;
    float    eyeZ          = 0.0f;
    float    forwardMm     = 0.0f;
    float    monitorLiftMm = 0.0f;
    float    sceneMin[3]   = {};
    float    sceneMax[3]   = {};
    float    deviceMin[3]  = {};
    float    deviceMax[3]  = {};
    float    driveTx[2]    = {};



    out = {};

    CBRAEx (driveCount >= 0 && driveCount <= 2, E_INVALIDARG);
    CBRAEx (dpi > 0, E_INVALIDARG);

    BAIL_OUT_IF (viewportW <= 0 || viewportH <= 0, S_FALSE);   // EHM-ALLOW-SFALSE: minimized/zero viewport is a routine skip-this-frame state the caller tests for, not an error

    out.viewportPx = viewportPx;
    out.driveCount = driveCount;
    aspect         = (float) viewportW / (float) viewportH;
    tanHalfX       = tanHalfY * aspect;

    // Monitor: centered on x = 0, front at z = 0, standing ON the drives --
    // its feet land on their lids, so the whole stack is one solid.
    //
    // ...unless the drive's door RISES, and then the stack opens up by
    // exactly what the door needs. The //c's latch travels straight up out of
    // its notch and clears the lid; with the monitor sitting flat on that lid
    // the two share the same millimeters, and the forward clearance alone is
    // a near miss you have to trust rather than a gap you can see. Lifting by
    // the rise plus a margin buys back the gap in the axis the motion is
    // actually in.
    monitorLiftMm = (driveCount > 0) ? (metrics.driveMax[2] - metrics.driveMin[2]) : 0.0f;

    if (driveCount > 0 && metrics.driveDoorRiseMm > 0.0f)
    {
        monitorLiftMm += metrics.driveDoorRiseMm + kDoorRiseMarginMm;
    }

    MakeDeviceWorld (-monitorCx, monitorLiftMm, 0.0f, 1.0f, out.monitorWorld);

    // Drives: a forward row on the ground plane -- toward the viewer, so
    // they read below the monitor from the straight-ahead camera -- at the
    // band's placement scale. One drive centers; two flank the centerline
    // with a gap.
    // The period stack: the drives sit side by side on the desk and the
    // MONITOR SITS ON TOP OF THEM, which is how these desks were actually
    // built (and how the reference photos are stacked). Two arrangements
    // were tried and rejected first, both for the same reason -- where a
    // device stands decides how distorted it looks. In front of the monitor,
    // a Disk II a foot nearer the eye projects at twice the monitor's
    // apparent size and swamps the picture. Flanking it, the drives sit far
    // enough off-axis that the fov needed to contain them shows each one's
    // whole 220 mm flank and lid, which reads as a crazily elongated box.
    // Stacked under the monitor they are near the view axis and barely below
    // eye level, so the front face is what you see -- as in the photographs.
    //
    // Every front face sits in ONE plane, the monitor's FRAME: a stack whose
    // parts stand at different distances reads as an accident, and it costs
    // the scene depth for nothing -- the nearer part just looks oversized.
    //
    // The frame, NOT the monitor's frontmost point. This monitor's inner bezel
    // stands proud of its frame by design and the tube bulges past even that,
    // so measuring the mesh marched the drives most of an inch toward the
    // viewer -- out of the plane the stack is supposed to share. Each model
    // names its own frame plane; see DeskSceneModel::FrontPlaneY.
    driveTx[0] = (driveCount == 2) ? -(driveW + kDriveGapMm) * 0.5f : 0.0f;
    driveTx[1] = (driveW + kDriveGapMm) * 0.5f;
    forwardMm  = metrics.driveFrontY - metrics.monitorFrontY;

    // ...except by whatever the drive's door needs to open. The shared
    // front plane is the rule and this is its one exception: a latch that
    // rises above the lid has the monitor in its way, and no amount of
    // coplanarity is worth a door that opens through the thing above it.
    forwardMm += metrics.driveDoorClearMm;

    (void) dropMm;

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
    // TWO bounds are tracked, and the difference matters.
    //
    // `scene` includes each device's contact-shadow ground clearance and is
    // what the camera fit must contain, or the shadows are clipped away.
    // `device` is the hardware alone, and is what the composition REPORTS as
    // its footprint. Ctrl+0 shrink-wraps the window around that footprint, so
    // reporting the padded bounds sized the window around a skirt of empty
    // floor -- a wide band of dead space down both sides and along the
    // bottom, which is exactly where the ground pads lie.
    deviceMin[0] = metrics.monitorMin[0] - monitorCx;
    deviceMax[0] = metrics.monitorMax[0] - monitorCx;
    deviceMin[1] = metrics.monitorMin[2] + monitorLiftMm;
    deviceMax[1] = metrics.monitorMax[2] + monitorLiftMm;
    deviceMin[2] = -metrics.monitorMax[1];
    deviceMax[2] = -metrics.monitorMin[1];

    // A device STANDING ON another device has no ground shadow to reserve
    // for: the monitor's would fall on the drive lids it sits on, hidden
    // behind the drives themselves. Reserving it anyway cost the frame a
    // wide band of empty floor on three sides for a shadow nobody can see.
    {
        float  monPadSide  = (monitorLiftMm > 0.0f) ? 0.0f : metrics.monitorPadSideMm;
        float  monPadDepth = (monitorLiftMm > 0.0f) ? 0.0f : metrics.monitorPadDepthMm;

        sceneMin[0] = deviceMin[0] - monPadSide;
        sceneMax[0] = deviceMax[0] + monPadSide;
        sceneMin[1] = deviceMin[1];
        sceneMax[1] = deviceMax[1];
        sceneMin[2] = deviceMin[2];
        sceneMax[2] = deviceMax[2] + monPadDepth;
    }

    for (int i = 0; i < driveCount; i++)
    {
        float   dlo[3] = { metrics.driveMin[0] - driveCx + driveTx[i],
                           metrics.driveMin[2],
                           forwardMm - metrics.driveMax[1] };
        float   dhi[3] = { metrics.driveMax[0] - driveCx + driveTx[i],
                           metrics.driveMax[2],
                           forwardMm - metrics.driveMin[1] };
        float   lo[3]  = { dlo[0] - metrics.drivePadSideMm, dlo[1], dlo[2] };
        float   hi[3]  = { dhi[0] + metrics.drivePadSideMm, dhi[1],
                           dhi[2] + metrics.drivePadDepthMm };

        for (int axis = 0; axis < 3; axis++)
        {
            deviceMin[axis] = std::min (deviceMin[axis], dlo[axis]);
            deviceMax[axis] = std::max (deviceMax[axis], dhi[axis]);
            sceneMin[axis]  = std::min (sceneMin[axis],  lo[axis]);
            sceneMax[axis]  = std::max (sceneMax[axis],  hi[axis]);
        }
    }

    // The camera looks at the glass center from slightly above (a person at
    // a desk), so top surfaces show and every device picks up its position's
    // parallax automatically. The straight-axis closed form seeds the
    // standoff; a few refine passes then project the actual scene corners
    // and tighten distance AND vertical aim until the scene fills the
    // viewport with just the contain margin -- without this the symmetric
    // frustum wastes the whole top margin on a bottom-heavy scene.
    glassCy = (metrics.glass.z0 + metrics.glass.z1) * 0.5f + monitorLiftMm;

    // The eye is PLACED, not solved: a seated person kViewingDistanceMm from
    // the monitor's front plane, sitting kEyeAboveMonitorTopMm above the
    // monitor's top, looking at the middle of the screen. Every perspective
    // in the frame follows from that one position.
    {
        float   at[3]     = { 0.0f, glassCy, 0.0f };
        float   eyeUp     = metrics.monitorMax[2] + monitorLiftMm + kEyeAboveMonitorTopMm;
        float   backOff   = 1.0f;
        float   fovY      = 0.0f;

        // Place the eye, solve the fov that shows the whole scene from there,
        // and correct two things per pass.
        //
        // The GAZE re-centers. A symmetric frustum aimed at the middle of the
        // screen has to reach from there down past the drives standing in
        // front of the desk, and then wastes exactly as much frame above the
        // monitor as it spent below -- most of the window empty. Tilting the
        // aim to the middle of what is actually there halves the fov needed
        // and fills the frame. The eye does not move: this is where the
        // person is looking, not where they are sitting.
        //
        // And if the fov still exceeds what anyone takes in at once, they
        // LEAN BACK. Moving the eye away along its own sight line is the
        // physical answer to "it does not all fit", and it keeps a
        // pathological viewport from turning the frame into a fisheye.
        for (int pass = 0; pass < 4; pass++)
        {
            float   eye[3]   = { 0.0f, glassCy + (eyeUp - glassCy) * backOff, kViewingDistanceMm * backOff };
            float   needTanX = 0.0f;
            float   needTanY = 0.0f;
            float   tanLo    = FLT_MAX;
            float   tanHi    = -FLT_MAX;

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

                tanLo = std::min (tanLo, view[1] / depth);
                tanHi = std::max (tanHi, view[1] / depth);
            }

            // Fit the tighter axis: a wide window is bounded by height, a
            // tall one by width.
            tanHalfY = std::max (needTanY, needTanX / aspect) * kContainMargin;
            tanHalfX = tanHalfY * aspect;

            eyeY = eye[1];
            eyeZ = eye[2];

            if (tanHalfY > std::tan (kMaxFovY * 0.5f))
            {
                backOff *= tanHalfY / std::tan (kMaxFovY * 0.5f);
                continue;
            }

            // Tilt the gaze so the scene's vertical span sits centered, then
            // re-solve against the new aim. Converges in a pass or two; a
            // negligible correction means it already is centered.
            if (tanLo <= tanHi)
            {
                float   midTan = (tanLo + tanHi) * 0.5f;

                if (std::abs (midTan) <= 0.002f)
                {
                    break;
                }

                at[1] += midTan * kViewingDistanceMm * backOff;
            }
            else
            {
                break;
            }
        }

        // THE ORBIT ROTATES THE SCENE, NOT THE CAMERA. The eye stays in its
        // solved seat and the DEVICES turn about the gaze target -- yaw about
        // world up, pitch about the screen's horizontal through the same
        // point. On screen the two are the same turntable; under the LIGHTS
        // they are not. The room lights live at fixed world positions, so a
        // swinging camera kept every face lit exactly as it was and the whole
        // rotation read as a trick -- shadows frozen mid-turn. With the
        // models actually turning, the world matrices carry the rotation into
        // the lighting and the shadow passes for free, and the light falls
        // where the new pose says it should.
        //
        // The angles are the INVERSE of the eye swing they replace, so every
        // gesture keeps the direction it had: rotating the camera one way and
        // the world the other way are the same picture.
        //
        // The fov stays frontal, solved for the unrotated pose: re-solving
        // containment per angle would make the scene breathe as it spins,
        // which is worse than the cropping it prevents. At extreme angles the
        // answer is the zoom and pan the user already has.
        if (view.orbitYawRad != 0.0f || view.orbitPitchRad != 0.0f)
        {
            float  cy = std::cos (-view.orbitYawRad);
            float  sy = std::sin (-view.orbitYawRad);
            float  cp = std::cos (view.orbitPitchRad);
            float  sp = std::sin (view.orbitPitchRad);

            // RotY(-yaw) * RotX(+pitch), row-vector convention, then the
            // pivot carried through and put back: p' = (p - at) * R + at.
            float  rot[16] =
            {
                cy,        sy * sp,        -sy * cp,       0.0f,
                0.0f,      cp,              sp,            0.0f,
                sy,        -cy * sp,        cy * cp,       0.0f,
                0.0f,      0.0f,            0.0f,          1.0f,
            };

            rot[12] = at[0] - (at[0] * rot[0] + at[1] * rot[4] + at[2] * rot[8]);
            rot[13] = at[1] - (at[0] * rot[1] + at[1] * rot[5] + at[2] * rot[9]);
            rot[14] = at[2] - (at[0] * rot[2] + at[1] * rot[6] + at[2] * rot[10]);

            {
                float  rotated[16] = {};

                SceneCamera::Mul44 (out.monitorWorld, rot, rotated);
                memcpy (out.monitorWorld, rotated, sizeof (rotated));

                for (int i = 0; i < out.driveCount; i++)
                {
                    SceneCamera::Mul44 (out.driveWorld[i], rot, rotated);
                    memcpy (out.driveWorld[i], rotated, sizeof (rotated));
                }
            }
        }

        fovY = std::clamp (2.0f * std::atan (tanHalfY), kMinFovY, kMaxFovY);

        SceneCamera::PerspectiveFovRH (fovY, aspect, kNearMm, kFarMm, out.proj);

        // The user's framing goes on HERE -- after the containment solve has
        // had its say and before anything is projected -- so every rect below
        // is measured through the same lens the scene is drawn through.
        ApplyViewTransform (view, out.proj);

        SceneCamera::Mul44            (out.view, out.proj, out.viewProj);
    }

    {

        // The HARDWARE's projected footprint, for aspect-matched sizing
        // (Ctrl+0 shrink-wraps the window around it). Deliberately the
        // device bounds and not the padded scene: wrapping the window around
        // the contact shadows' ground clearance surrounds the machine with a
        // band of empty floor on the two sides and the near edge.
        {
            float   pxMin[2] = { FLT_MAX, FLT_MAX };
            float   pxMax[2] = { -FLT_MAX, -FLT_MAX };
            bool    all      = true;

            for (int corner = 0; corner < 8; corner++)
            {
                float   pt[3] = { (corner & 1) ? deviceMax[0] : deviceMin[0],
                                  (corner & 2) ? deviceMax[1] : deviceMin[1],
                                  (corner & 4) ? deviceMax[2] : deviceMin[2] };
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

        // The label anchor: the drive's front-bottom center, one FIXED model
        // point rather than the box's swelling projected bounds.
        if (i >= 0 && all)
        {
            float   anchor[3]  = { (boxMin[0] + boxMax[0]) * 0.5f, boxMin[1], boxMin[2] };
            float   worldPt[3] = {};
            float   px[2]      = {};

            if (SceneCamera::TransformPoint (world, anchor, worldPt) &&
                SceneCamera::ProjectToScreen (out.viewProj, worldPt, viewportPx, px))
            {
                out.driveLabelPx[i].x = (LONG) std::lround (px[0]);
                out.driveLabelPx[i].y = (LONG) std::lround (px[1]);

                out.driveLabelWorld[i][0] = worldPt[0];
                out.driveLabelWorld[i][1] = worldPt[1];
                out.driveLabelWorld[i][2] = worldPt[2];
            }
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
//  DeskSceneLayout::SolvePictureStandoff
//
//  The band's boundary sampled at its corners AND its edge midpoints: the
//  midpoints are where the sag stands proudest, and the standoff a point
//  demands grows with how far forward it sits, so an edge's middle asks for
//  more room than the corners it runs between. Three samples an edge is
//  therefore exact for a spherical sag rather than an approximation -- along
//  any one edge the off-axis distance is constant and the forward bulge peaks
//  at the middle.
//
////////////////////////////////////////////////////////////////////////////////

float DeskSceneLayout::SolvePictureStandoff (const DeskSceneMetrics & metrics,
                                             const float              monitorWorld[16],
                                             int                      displayW,
                                             int                      displayH,
                                             float                    aspect)
{
    constexpr int   kEdgeSamples = 3;   // both ends and the middle
    float           monitorCx    = (metrics.monitorMin[0] + metrics.monitorMax[0]) * 0.5f;
    float           glassCx      = (metrics.glass.x0 + metrics.glass.x1) * 0.5f - monitorCx;
    float           glassCy      = (metrics.glass.z0 + metrics.glass.z1) * 0.5f;
    float           planeZ       = -metrics.glass.baseY;
    float           bandU0       = 0.0f;
    float           bandV0       = 0.0f;
    float           bandU1       = 1.0f;
    float           bandV1       = 1.0f;
    float           standoff     = 0.0f;



    CurvedDisplayMath::ComputePictureBand (metrics.glass, displayW, displayH,
                                           bandU0, bandV0, bandU1, bandV1);

    for (int iu = 0; iu < kEdgeSamples; iu++)
    {
        for (int iv = 0; iv < kEdgeSamples; iv++)
        {
            float   fu         = (float) iu / (float) (kEdgeSamples - 1);
            float   fv         = (float) iv / (float) (kEdgeSamples - 1);
            float   modelPt[3] = {};
            float   worldPt[3] = {};

            CurvedDisplayMath::ModelPointFromUv (metrics.glass,
                                                 bandU0 + fu * (bandU1 - bandU0),
                                                 bandV0 + fv * (bandV1 - bandV0),
                                                 modelPt);

            if (!SceneCamera::TransformPoint (monitorWorld, modelPt, worldPt))
            {
                continue;
            }

            standoff = std::max (standoff,
                                 SceneCamera::SolveContainStandoff (worldPt[0] - glassCx,
                                                                    worldPt[1] - glassCy,
                                                                    worldPt[2] - planeZ,
                                                                    kFovY, aspect));
        }
    }

    return standoff;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::ComputeGlassFill
//
//  The fullscreen camera: the monitor mounts exactly as in the windowed
//  composition (so the glass surface, hit tester, and picture band all keep
//  working unchanged), and the camera stands as CLOSE to it as it can get
//  without cutting anything off the picture.
//
//  Close means covering the viewport with the glass, which is what keeps the
//  monitor's case off screen. But the glass is only about 1.42 to 1 and the
//  raster all but fills it, so covering a WIDER viewport than that crops the
//  glass vertically and takes the picture with it: on 16:9 that was 14% of
//  the picture's height, a dozen scanlines off each end, in text as much as in
//  graphics. So the cover standoff is a floor the picture is allowed to
//  raise, not the answer: up to about 3:2 the glass covers exactly as it
//  always did, and past that the eye backs off just far enough to hold the
//  whole raster.
//
//  What that leaves beside the tube is BLACK, not the monitor. The
//  composition carries `glassOnly` and DeskScene::RenderPlate drops the case,
//  the bezel and the lamp for it -- otherwise backing the camera off shows a
//  case cropped at the top and bottom but not the sides, which reads as a
//  monitor with its middle sliced out rather than as a monitor.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DeskSceneLayout::ComputeGlassFill (const RECT             & viewportPx,
                                           UINT                     dpi,
                                           int                      displayW,
                                           int                      displayH,
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
    float     tanY      = std::tan (kFovY * 0.5f);
    float     aspect    = 0.0f;
    float     standoff  = 0.0f;
    float     halfWpx   = 0.0f;
    float     halfHpx   = 0.0f;
    float     nativeHPx = 0.0f;



    out = {};

    CBRAEx (dpi > 0, E_INVALIDARG);
    CBRAEx (displayW > 0 && displayH > 0, E_INVALIDARG);

    BAIL_OUT_IF (viewportW <= 0 || viewportH <= 0, S_FALSE);   // EHM-ALLOW-SFALSE: minimized/zero viewport is a routine skip-this-frame state the caller tests for, not an error

    out.viewportPx = viewportPx;
    out.driveCount = 0;
    out.glassOnly  = 1;
    aspect         = (float) viewportW / (float) viewportH;

    MakeDeviceWorld (-monitorCx, 0.0f, 0.0f, 1.0f, out.monitorWorld);

    standoff = std::max (SceneCamera::SolveCoverStandoff (glassW, glassH, kFovY, aspect),
                         SolvePictureStandoff (metrics, out.monitorWorld, displayW, displayH, aspect));

    SceneCamera::SolveStraightOnCamera (glassCx, glassCy, -metrics.glass.baseY, standoff,
                                        kFovY, aspect, kNearMm, kFarMm,
                                        out.view, out.proj, out.viewProj);

    // Straight on at the glass rect's own center, so the rect projects
    // centered and square and needs no corner pass -- its half-extents are
    // just the off-axis distances over the frustum's span at the standoff.
    // Clamped to the viewport, because a covering glass runs off it and what
    // this rect reports is the CRT's room ON SCREEN.
    halfWpx = std::min ((float) viewportW * 0.5f,
                        (glassW * 0.5f) / (standoff * tanY * aspect) * ((float) viewportW * 0.5f));
    halfHpx = std::min ((float) viewportH * 0.5f,
                        (glassH * 0.5f) / (standoff * tanY) * ((float) viewportH * 0.5f));

    {
        float  centerX = (float) (viewportPx.left + viewportPx.right) * 0.5f;
        float  centerY = (float) (viewportPx.top + viewportPx.bottom) * 0.5f;

        out.glassRectPx.left   = (LONG) std::floor (centerX - halfWpx);
        out.glassRectPx.top    = (LONG) std::floor (centerY - halfHpx);
        out.glassRectPx.right  = (LONG) std::ceil  (centerX + halfWpx);
        out.glassRectPx.bottom = (LONG) std::ceil  (centerY + halfHpx);
    }

    out.sceneRectPx = viewportPx;

    nativeHPx      = (float) kScreenNativeHDp * (float) dpi / 96.0f;
    out.sceneScale = (halfHpx * 2.0f) / nativeHPx;

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

        // The label anchor, same as the desk composition fills it -- unused
        // by the strip today, but the field promises a value wherever the
        // drive projected.
        if (all)
        {
            float   anchor[3]  = { (metrics.driveMin[0] + metrics.driveMax[0]) * 0.5f,
                                   metrics.driveMin[1], metrics.driveMin[2] };
            float   worldPt[3] = {};
            float   px[2]      = {};

            if (SceneCamera::TransformPoint (out.driveWorld[i], anchor, worldPt) &&
                SceneCamera::ProjectToScreen (out.viewProj, worldPt, viewportPx, px))
            {
                out.driveLabelPx[i].x = (LONG) std::lround (px[0]);
                out.driveLabelPx[i].y = (LONG) std::lround (px[1]);

                out.driveLabelWorld[i][0] = worldPt[0];
                out.driveLabelWorld[i][1] = worldPt[1];
                out.driveLabelWorld[i][2] = worldPt[2];
            }
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





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::GetCameraBasis
//
//  Reads the camera's right and up axes out of the view matrix.
//
//  The rotation block holds the camera basis in its COLUMNS under this file's
//  row-vector convention, because a view transform is the inverse of the
//  camera's placement and a rotation's inverse is its transpose. So right is
//  (m0, m4, m8) and up is (m1, m5, m9), already unit length and mutually
//  square.
//
//  The orbit turns the MODELS rather than the eye, so both axes hold still
//  through a whole orbit. A quad built on them keeps its facing however far
//  the scene has been spun, which is what a name that has to stay readable
//  wants.
//
////////////////////////////////////////////////////////////////////////////////

void DeskSceneLayout::GetCameraBasis (const float view[16], float outRight[3], float outUp[3])
{
    outRight[0] = view[0];
    outRight[1] = view[4];
    outRight[2] = view[8];

    outUp[0]    = view[1];
    outUp[1]    = view[5];
    outUp[2]    = view[9];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::GetWorldPerPixel
//
//  Inverts the perspective divide at one depth: how much world a single pixel
//  covers there.
//
//  Clip w under this projection IS the distance in front of the eye, so it
//  falls straight out of the fourth column. Screen position is (v * m11 / w)
//  scaled by half the viewport, which rearranges to 2w / (viewport * m11)
//  world units per pixel.
//
//  Taken from the composition's own proj rather than from kFovY, so the
//  clip-space zoom ApplyViewTransform folded in is already counted. Sizing
//  through the fov instead would be right at zoom 1.0 and wrong everywhere
//  else.
//
////////////////////////////////////////////////////////////////////////////////

bool DeskSceneLayout::GetWorldPerPixel (const DeskSceneComposition & comp,
                                        const float                  worldPt[3],
                                        float                      & outPerPxX,
                                        float                      & outPerPxY)
{
    float  w         = worldPt[0] * comp.viewProj[3]  + worldPt[1] * comp.viewProj[7] +
                       worldPt[2] * comp.viewProj[11] + comp.viewProj[15];
    int    viewportW = comp.viewportPx.right  - comp.viewportPx.left;
    int    viewportH = comp.viewportPx.bottom - comp.viewportPx.top;



    outPerPxX = 0.0f;
    outPerPxY = 0.0f;

    if (w <= 0.0f || viewportW <= 0 || viewportH <= 0 ||
        comp.proj[0] <= 0.0f || comp.proj[5] <= 0.0f)
    {
        return false;
    }

    outPerPxX = 2.0f * w / ((float) viewportW * comp.proj[0]);
    outPerPxY = 2.0f * w / ((float) viewportH * comp.proj[5]);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneLayout::TryMakeDriveLabelQuad
//
//  Places the name's billboard: a rectangle square to the camera, hung under
//  the drive's anchor, covering the pixel size the caller asked for.
//
//  CONSTANT SCREEN SIZE IS THE POINT. Standing the name in the drive's own
//  plane made it foreshorten and shrink with the pose, so the one thing on
//  screen whose job is to be read went unreadable on a small desk. Spanning
//  the quad with the camera's own axes and sizing it through the pixel scale
//  at its depth gives back the fixed size chrome had, while leaving the name
//  real geometry that the depth buffer can cut.
//
//  All four corners share ONE depth, because right and up are both square to
//  the gaze. That makes the rectangle exact rather than approximate: it
//  covers the requested pixels precisely, and it cannot tilt into the case it
//  is meant to be hidden by.
//
////////////////////////////////////////////////////////////////////////////////

bool DeskSceneLayout::TryMakeDriveLabelQuad (const DeskSceneComposition & comp,
                                             int                          drive,
                                             const SIZE                 & labelPx,
                                             int                          gapPx,
                                             float                        outCorners[4][3])
{
    const float  * anchor   = nullptr;
    float          right[3] = {};
    float          up[3]    = {};
    float          perPxX   = 0.0f;
    float          perPxY   = 0.0f;
    float          halfW    = (float) labelPx.cx * 0.5f;
    float          topPx    = (float) gapPx;
    float          botPx    = (float) (gapPx + labelPx.cy);
    float          offsX[4] = { -halfW, halfW, -halfW, halfW };
    float          offsY[4] = { topPx,  topPx, botPx,  botPx  };



    memset (outCorners, 0, sizeof (float) * 4 * 3);

    if (drive < 0 || drive >= 2 || drive >= comp.driveCount ||
        labelPx.cx <= 0 || labelPx.cy <= 0)
    {
        return false;
    }

    anchor = comp.driveLabelWorld[drive];

    if (!GetWorldPerPixel (comp, anchor, perPxX, perPxY))
    {
        return false;
    }

    GetCameraBasis (comp.view, right, up);

    for (int corner = 0; corner < 4; corner++)
    {
        float  dx = offsX[corner] * perPxX;
        float  dy = offsY[corner] * perPxY;



        // Screen y runs DOWN while the camera's up runs up, so the offset
        // below the anchor is subtracted rather than added.
        for (int axis = 0; axis < 3; axis++)
        {
            outCorners[corner][axis] = anchor[axis] + dx * right[axis] - dy * up[axis];
        }
    }

    return true;
}
