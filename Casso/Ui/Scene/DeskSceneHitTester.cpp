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
                                             const float *                      monitorWorld,
                                             const float *                      monitorBoundsMin,
                                             const float *                      monitorBoundsMax,
                                             const float *                      driveBoundsMin,
                                             const float *                      driveBoundsMax,
                                             const DeskRegionBox *              driveDoorBoxes)
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



    if (!SceneCamera::Inverse44 (comp.viewProj, invViewProj) ||
        !SceneCamera::ScreenRayFromPx (invViewProj, comp.viewportPx, screenX, screenY, origin, dir))
    {
        return result;
    }

    // OCCLUSION, before anything may claim. A click is a ray, and a ray that
    // reaches a drive's door THROUGH the monitor's case -- or through the
    // drive's own lid from behind -- is not a click on the door, however
    // squarely it lands on the region box. Each device's whole body is
    // ray-tested here, and the entry distances decide who stands in front:
    // a surface may only claim the click if no OTHER device's body begins
    // nearer along the ray.
    //
    // Bodies as bounds boxes rather than triangle meshes, deliberately: the
    // question is which DEVICE is in front, not which of its 250k triangles,
    // and the boxes are tight around cases that are themselves box-shaped.
    //
    // ABSENT BOUNDS MEAN NO OCCLUSION INFORMATION, NOT "NOTHING IS IN FRONT".
    // A caller that supplies no monitor body gets the pre-occlusion behavior:
    // the glass claims what it covers. Starting `monitorFrontal` at false
    // instead made the picture unclickable for every such caller, which is
    // the opposite of a refinement.
    float  tMonitorBody   = FLT_MAX;
    float  tDriveBody[2]  = { FLT_MAX, FLT_MAX };
    bool   monitorFrontal = (monitorBoundsMin == nullptr || monitorBoundsMax == nullptr);

    if (includeGlass && monitorBoundsMin != nullptr && monitorBoundsMax != nullptr)
    {
        float  invWorld[16]   = {};
        float  modelOrigin[3] = {};
        float  modelDir[3]    = {};
        float  tNear          = 0.0f;

        if (SceneCamera::Inverse44 (monWorld, invWorld) &&
            SceneCamera::TransformPoint (invWorld, origin, modelOrigin))
        {
            SceneCamera::TransformVector (invWorld, dir, modelDir);

            if (RayHitsBox (modelOrigin, modelDir, monitorBoundsMin, monitorBoundsMax, tNear))
            {
                tMonitorBody = tNear;

                // Model +Y is INTO the case from its face: a ray whose Y runs
                // positive approaches from the front. From behind, nothing on
                // the monitor -- least of all the picture -- is clickable.
                monitorFrontal = modelDir[1] > 0.0f;
            }
        }
    }

    if (driveBoundsMin != nullptr && driveBoundsMax != nullptr)
    {
        for (int drive = 0; drive < comp.driveCount; drive++)
        {
            float  invWorld[16]   = {};
            float  modelOrigin[3] = {};
            float  modelDir[3]    = {};
            float  tNear          = 0.0f;

            if (SceneCamera::Inverse44 (comp.driveWorld[drive], invWorld) &&
                SceneCamera::TransformPoint (invWorld, origin, modelOrigin))
            {
                SceneCamera::TransformVector (invWorld, dir, modelDir);

                if (RayHitsBox (modelOrigin, modelDir, driveBoundsMin, driveBoundsMax, tNear))
                {
                    tDriveBody[drive] = tNear;
                }
            }
        }
    }

    if (includeGlass && monitorFrontal &&
        CurvedDisplayMath::EmulatedPixelFromScreenPx (glass, monWorld, comp.viewProj,
                                                      comp.viewportPx, screenX, screenY,
                                                      displayW, displayH, result.emulatedPixel))
    {
        // The picture is the monitor's; a drive standing nearer along the
        // ray means the ray is not looking at the picture at all.
        bool  occluded = false;

        for (int drive = 0; drive < comp.driveCount; drive++)
        {
            occluded = occluded || (tDriveBody[drive] < tMonitorBody - kOcclusionSlackMm);
        }

        if (!occluded)
        {
            result.target = SceneHitResult::Target::Glass;
            return result;
        }

        result.emulatedPixel = POINT {};
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

        // From behind, a drive has no clickable face: its regions are front
        // furniture, and the box that describes one extends through air the
        // real part does not occupy.
        if (modelDir[1] <= 0.0f)
        {
            continue;
        }

        // THE DOOR FIRST, AND THE WHOLE DOOR. It is the one part of a drive
        // that moves, so its target is handed in posed rather than read from
        // the fixed list below -- and it is tested ahead of that list because
        // where it travels to, on the //c, is out over the lid and past every
        // box the case owns.
        if (driveDoorBoxes != nullptr)
        {
            const DeskRegionBox &  door  = driveDoorBoxes[drive];
            float                  tNear = 0.0f;
            float                  lo[3] = { door.boxMin[0], door.boxMin[1], door.boxMin[2] };
            float                  hi[3] = { door.boxMax[0], door.boxMax[1], door.boxMax[2] };

            if (hi[0] > lo[0])
            {
                lo[0] -= kDoorHitPadMm;       hi[0] += kDoorHitPadMm;
                lo[1] -= kDoorHitFrontPadMm;  hi[1] += kDoorHitPadMm;
                lo[2] -= kDoorHitPadMm;       hi[2] += kDoorHitPadMm;

                if (RayHitsBox (modelOrigin, modelDir, lo, hi, tNear) && tNear < bestT)
                {
                    bestT             = tNear;
                    result.target     = SceneHitResult::Target::Drive;
                    result.driveIndex = drive;
                    result.region     = DriveWidgetRegion::Eject;
                    continue;
                }
            }
        }

        for (const DeskRegionBox & box : driveRegions)
        {
            float   tNear = 0.0f;

            if (!RayHitsBox (modelOrigin, modelDir, box.boxMin, box.boxMax, tNear))
            {
                continue;
            }

            // Anything else standing nearer along the ray owns it: the
            // monitor's case, or the other drive's body. The device's OWN
            // body is exempt -- a door region rightly begins in front of the
            // case it is mounted on.
            {
                bool  occluded = tMonitorBody < tNear - kOcclusionSlackMm;

                for (int other = 0; other < comp.driveCount; other++)
                {
                    occluded = occluded ||
                               (other != drive && tDriveBody[other] < tNear - kOcclusionSlackMm);
                }

                if (occluded)
                {
                    continue;
                }
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
