#pragma once

#include "Pch.h"

#include "Render/CurvedDisplayMath.h"
#include "Ui/Scene/DeskSceneLayout.h"
#include "Ui/Scene/DeskSceneModel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DeskSceneHitTester
//
//  Screen px -> ray -> scene resolution: the curved glass (with its emulated
//  pixel), a drive region (slot/eject or body, nearest drive winning), or
//  nothing. Pure math over the composition and the models' declared geometry
//  -- the scene's replacement for the 2D widgets' rect HitTest, with
//  identical region semantics: the glass outranks region boxes, and within a
//  drive the boxes test in declaration order (eject before body).
//
////////////////////////////////////////////////////////////////////////////////

struct SceneHitResult
{
    enum class Target
    {
        None,
        Glass,
        Drive,
        BezelTilt,
    };

    Target             target        = Target::None;
    int                driveIndex    = -1;
    DriveWidgetRegion  region        = {};
    POINT              emulatedPixel = {};

    // Which tilt mark was grabbed: +1 the up one, -1 the down one. Only
    // meaningful for BezelTilt.
    int                tiltDirection = 0;
};


class DeskSceneHitTester
{
public:
    // Resolves one screen position against the composed scene. `glass` is
    // the monitor's surface (tested through comp.monitorWorld); the region
    // boxes are shared by every drive and tested through comp.driveWorld[i].
    // `includeGlass` false skips the glass entirely -- for drives-only
    // compositions (the fullscreen strip) whose monitor placement is
    // meaningless.
    static SceneHitResult  Classify (const DeskSceneComposition       & comp,
                                     const CurvedDisplaySurface       & glass,
                                     const std::vector<DeskRegionBox> & driveRegions,
                                     float                              screenX,
                                     float                              screenY,
                                     int                                displayW,
                                     int                                displayH,
                                     bool                               includeGlass = true,
                                     const std::vector<DeskTiltGrip> *  tiltGrips    = nullptr,
                                     const float *                      monitorWorld = nullptr,
                                     const float *                      monitorBoundsMin = nullptr,
                                     const float *                      monitorBoundsMax = nullptr,
                                     const float *                      driveBoundsMin   = nullptr,
                                     const float *                      driveBoundsMax   = nullptr);

private:
    // Slab test; reports the entry distance so drives can compete on
    // nearest-hit.
    // How much nearer another body must begin before it counts as standing
    // in the way. The devices touch -- the monitor sits ON the drives -- so
    // an exact comparison would let the neighbor's abutting face steal
    // clicks that land squarely on a door.
    static constexpr float  kOcclusionSlackMm = 2.0f;

    static bool  RayHitsBox (const float   origin[3],
                             const float   dir[3],
                             const float   boxMin[3],
                             const float   boxMax[3],
                             float       & outTNear);
};
