#include "Pch.h"

#include "Ui/Scene/DeskSceneHitTester.h"

#include "Render/SceneCamera.h"
#include "Ui/Chrome/DriveWidget.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneHitTester::RayHitsBox
//
//  Standard slab intersection. Division by a zero direction component yields
//  +/-inf, which the min/max comparisons handle correctly (the IEEE trick the
//  branchless form relies on); a NaN from 0/0 fails every comparison and
//  reports a miss, which is the safe answer for a degenerate ray.
//
////////////////////////////////////////////////////////////////////////////////

bool DeskSceneHitTester::RayHitsBox (const float   origin[3],
                                     const float   dir[3],
                                     const float   boxMin[3],
                                     const float   boxMax[3],
                                     float       & outTNear)
{
    float   tNear = -FLT_MAX;
    float   tFar  = FLT_MAX;



    for (int axis = 0; axis < 3; axis++)
    {
        float   t1 = (boxMin[axis] - origin[axis]) / dir[axis];
        float   t2 = (boxMax[axis] - origin[axis]) / dir[axis];

        tNear = std::max (tNear, std::min (t1, t2));
        tFar  = std::min (tFar, std::max (t1, t2));
    }

    if (!(tNear <= tFar) || tFar < 0.0f)
    {
        return false;
    }

    outTNear = std::max (tNear, 0.0f);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneHitTester::Classify
//
//  The glass is tested first and outranks everything: it is the one surface
//  whose hit carries a payload (the emulated pixel), and no device geometry
//  overlaps it in the composition. Drives then compete on nearest entry
//  distance, and within a drive the region boxes resolve in declaration
//  order -- eject before body, the 2D band's precedence.
//
////////////////////////////////////////////////////////////////////////////////

SceneHitResult DeskSceneHitTester::Classify (const DeskSceneComposition       & comp,
                                             const CurvedDisplaySurface       & glass,
                                             const std::vector<DeskRegionBox> & driveRegions,
                                             float                              screenX,
                                             float                              screenY,
                                             int                                displayW,
                                             int                                displayH,
                                             bool                               includeGlass,
                                             const std::vector<DeskTiltGrip> *  tiltGrips,
                                             const float *                      monitorWorld)
{
    SceneHitResult   result;
    float            invViewProj[16] = {};
    float            origin[3]       = {};
    float            dir[3]          = {};
    float            bestT           = FLT_MAX;

    // The monitor's placement, tilt and all. The caller hands one in when the
    // bezel has moved, because the tube and its marks travel with it -- test
    // them against where they were drawn, not against where the untilted
    // model puts them.
    const float *    monWorld        = (monitorWorld != nullptr) ? monitorWorld : comp.monitorWorld;



    if (includeGlass &&
        CurvedDisplayMath::EmulatedPixelFromScreenPx (glass, monWorld, comp.viewProj,
                                                      comp.viewportPx, screenX, screenY,
                                                      displayW, displayH, result.emulatedPixel))
    {
        result.target = SceneHitResult::Target::Glass;
        return result;
    }

    if (!SceneCamera::Inverse44 (comp.viewProj, invViewProj) ||
        !SceneCamera::ScreenRayFromPx (invViewProj, comp.viewportPx, screenX, screenY, origin, dir))
    {
        return result;
    }

    // The bezel's tilt marks. They are on the monitor rather than on a drive,
    // so they get their own pass -- and they join the same nearest-wins
    // contest, so a drive standing in front of the monitor still takes the
    // click.
    if (tiltGrips != nullptr && !tiltGrips->empty())
    {
        float   invWorld[16]   = {};
        float   modelOrigin[3] = {};
        float   modelDir[3]    = {};

        if (SceneCamera::Inverse44 (monWorld, invWorld) &&
            SceneCamera::TransformPoint (invWorld, origin, modelOrigin))
        {
            SceneCamera::TransformVector (invWorld, dir, modelDir);

            for (const DeskTiltGrip & grip : *tiltGrips)
            {
                float   tNear = 0.0f;

                if (!RayHitsBox (modelOrigin, modelDir, grip.boxMin, grip.boxMax, tNear))
                {
                    continue;
                }

                if (tNear < bestT)
                {
                    bestT                = tNear;
                    result.target        = SceneHitResult::Target::BezelTilt;
                    result.tiltDirection = grip.direction;
                    result.driveIndex    = -1;
                }
            }
        }
    }

    for (int drive = 0; drive < comp.driveCount; drive++)
    {
        float   invWorld[16]   = {};
        float   modelOrigin[3] = {};
        float   modelDir[3]    = {};

        if (!SceneCamera::Inverse44 (comp.driveWorld[drive], invWorld) ||
            !SceneCamera::TransformPoint (invWorld, origin, modelOrigin))
        {
            continue;
        }

        SceneCamera::TransformVector (invWorld, dir, modelDir);

        for (const DeskRegionBox & box : driveRegions)
        {
            float   tNear = 0.0f;

            if (!RayHitsBox (modelOrigin, modelDir, box.boxMin, box.boxMax, tNear))
            {
                continue;
            }

            if (tNear < bestT)
            {
                bestT             = tNear;
                result.target     = SceneHitResult::Target::Drive;
                result.driveIndex = drive;
                result.region     = box.region;
            }

            // Declaration order is precedence WITHIN a drive: the first box
            // hit settles this drive's region, and only a strictly nearer
            // drive can override it.
            break;
        }
    }

    return result;
}
