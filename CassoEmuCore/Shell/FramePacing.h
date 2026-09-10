#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FrameSignature
//
//  Everything about the guest that can change what the renderer produces.
//
//  Video-dirty comes from the bus: a write into a display page, or a banking
//  change that moves which page is on screen. The other three are soft-switch
//  state the bus cannot see change, because reading a switch is what moves it
//  and nothing is written.
//
////////////////////////////////////////////////////////////////////////////////

struct FrameSignature
{
    bool      videoDirty = false;
    uint32_t  modeSig    = 0;
    bool      flashOn    = false;
    uint64_t  colorSig   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  FramePacing
//
//  The two decisions that stand between the emulation advancing and a picture
//  reaching the screen: whether to publish this frame at all, and whether
//  anything about the picture actually changed.
//
//  Both used to be a few lines inside the CPU thread's per-frame callback,
//  where they could only be exercised by running the emulator and watching it.
//  They are arithmetic over their inputs, so they are here instead, where a
//  test can hand them a steady screen and assert that nothing re-rasterizes.
//
//  Getting either wrong is felt rather than seen -- a dropped frame, a stutter,
//  a screen that will not refresh -- which is the class of bug a test holds
//  much better than an eye does.
//
////////////////////////////////////////////////////////////////////////////////

class FramePacing
{
public:

    //  Below Maximum speed the CPU thread is already paced to one frame per
    //  vsync by its waitable timer, so every frame publishes. At Maximum speed
    //  emulation is unthrottled and the publish is held to a wall-clock
    //  cadence, or the renderer would be asked for frames faster than a
    //  display can show them.
    //
    //  `sinceLastPublishUs` is measured by the caller, which owns the clock.
    //  Time is the one thing a test may not fake by touching the real one.
    static bool  ShouldPublish (bool      isMaximumSpeed,
                                int64_t   sinceLastPublishUs,
                                int64_t   minIntervalUs);

    //  A steady screen re-rasterizes nothing. Any one input moving is enough
    //  to require a re-render: the mode, the flash phase, the color treatment,
    //  or the bus reporting a write into a display page.
    static bool  NeedsRender (const FrameSignature & current,
                              const FrameSignature & lastRendered);
};
