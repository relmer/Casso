#include "Pch.h"

#include "Shell/FramePacing.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FramePacing::ShouldPublish
//
//  Whether this frame reaches the renderer.
//
//  Only Maximum speed is throttled. Every slower mode is already paced by the
//  CPU thread's waitable timer, and throttling a second time there would drop
//  frames the emulation had correctly produced on time.
//
////////////////////////////////////////////////////////////////////////////////

bool FramePacing::ShouldPublish (
    bool     isMaximumSpeed,
    int64_t  sinceLastPublishUs,
    int64_t  minIntervalUs)
{
    bool  shouldPublish = !isMaximumSpeed || sinceLastPublishUs >= minIntervalUs;



    return (shouldPublish);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FramePacing::NeedsRender
//
//  Whether the picture can have changed since the last one drawn.
//
//  The comparison is against what was last RENDERED rather than what was last
//  seen, which matters at Maximum speed: frames skipped by the publish gate
//  never update the remembered signature, so a change that happens during a
//  skipped frame still forces a render on the next published one instead of
//  being lost.
//
////////////////////////////////////////////////////////////////////////////////

bool FramePacing::NeedsRender (
    const FrameSignature & current,
    const FrameSignature & lastRendered)
{
    bool  needsRender = current.videoDirty
                        || current.modeSig  != lastRendered.modeSig
                        || current.flashOn  != lastRendered.flashOn
                        || current.colorSig != lastRendered.colorSig;



    return (needsRender);
}
