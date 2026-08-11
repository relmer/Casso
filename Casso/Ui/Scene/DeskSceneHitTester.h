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
    };

    Target             target        = Target::None;
    int                driveIndex    = -1;
    DriveWidgetRegion  region        = {};
    POINT              emulatedPixel = {};
};


class DeskSceneHitTester
{
public:
    // Resolves one screen position against the composed scene. `glass` is
    // the monitor's surface (tested through comp.monitorWorld); the region
    // boxes are shared by every drive and tested through comp.driveWorld[i].
    static SceneHitResult  Classify (const DeskSceneComposition       & comp,
                                     const CurvedDisplaySurface       & glass,
                                     const std::vector<DeskRegionBox> & driveRegions,
                                     float                              screenX,
                                     float                              screenY,
                                     int                                displayW,
                                     int                                displayH);

private:
    // Slab test; reports the entry distance so drives can compete on
    // nearest-hit.
    static bool  RayHitsBox (const float   origin[3],
                             const float   dir[3],
                             const float   boxMin[3],
                             const float   boxMax[3],
                             float       & outTNear);
};
