#pragma once

#include "Pch.h"

#include "Render/CurvedDisplayMath.h"
#include "Ui/Scene/DeskSceneModel.h"

struct DeskSceneComposition;
class DeskSceneLayout;





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
        Recorder,      // the cassette recorder beside the stack
    };

    Target             target        = Target::None;
    int                driveIndex    = -1;
    DriveWidgetRegion  region        = {};
    POINT              emulatedPixel = {};

    // Which tilt mark was grabbed: +1 the up one, -1 the down one. Only
    // meaningful for BezelTilt.
    int                tiltDirection = 0;

    // Which of the recorder's keys, left to right, or -1 for the rest of the
    // case. Only meaningful for Recorder.
    int                recorderKey   = -1;
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
                                     const float *                      driveBoundsMax   = nullptr,
                                     const DeskRegionBox *              driveDoorBoxes   = nullptr,
                                     const float *                      recorderBoundsMin = nullptr,
                                     const float *                      recorderBoundsMax = nullptr,
                                     const float *                      recorderKeyBoxes  = nullptr,
                                     size_t                             recorderKeyCount  = 0);

private:
    // Slab test; reports the entry distance so drives can compete on
    // nearest-hit.
    // How much nearer another body must begin before it counts as standing
    // in the way. The devices touch -- the monitor sits ON the drives -- so
    // an exact comparison would let the neighbor's abutting face steal
    // clicks that land squarely on a door.
    static constexpr float  kOcclusionSlackMm = 2.0f;

    // Slack around the door's own geometry, so the target is the door and the
    // reach for it rather than the door's silhouette to the millimeter. The
    // front figure is deep because the door stands proud of the face and a
    // click a little in front of it plainly means the door.
    static constexpr float  kDoorHitPadMm      = 4.0f;
    static constexpr float  kDoorHitFrontPadMm = 8.0f;

    static bool  RayHitsBox (const float   origin[3],
                             const float   dir[3],
                             const float   boxMin[3],
                             const float   boxMax[3],
                             float       & outTNear);

    // The recorder is one target, its whole bounds box, tested through
    // comp.recorderWorld. It is in the same nearest-hit test the drives use,
    // and occluderT is the nearest entry of any other device's body.
    static void  ClassifyRecorder (const DeskSceneComposition & comp,
                                   const float                  origin[3],
                                   const float                  dir[3],
                                   const float                  boxMin[3],
                                   const float                  boxMax[3],
                                   float                        occluderT,
                                   float                      & bestT,
                                   SceneHitResult             & result,
                                   const float *                keyBoxes,
                                   size_t                       keyCount);

    // A key is a small target on a big case, so its box reaches a little above
    // the key, so a click aimed at its top still hits it.
    static constexpr float  kKeyHitPadMm = 2.0f;
};
