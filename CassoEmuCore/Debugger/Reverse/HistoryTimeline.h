#pragma once

#include "Pch.h"

class KeyframeStore;
struct StretchSummary;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryTimeline
//
//  Facts about history from a keyframe store's keyframes alone, without a
//  replay: the stretch each position is in, what a stretch did, and the
//  host's clock at a cycle or a position. The newest stretch ends at
//  newestEnd, the end of history, which the caller passes in.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryTimeline
{
public:
    //  The positions one keyframe's replay covers: from the keyframe to the
    //  next one, or to where recording paused, or to the end of history.
    struct Stretch
    {
        size_t    keyframe = 0;
        uint64_t  start    = 0;
        uint64_t  end      = 0;
    };

    explicit HistoryTimeline (const KeyframeStore & keyframes);

    uint64_t  GetWallTimeAt         (uint64_t cycle) const;
    uint64_t  GetWallTimeAtPosition (uint64_t position, uint64_t cycle) const;
    bool      TryGetSummary         (size_t keyframe, StretchSummary & outSummary) const;
    bool      TryFindStretch        (uint64_t position, uint64_t newestEnd, Stretch & outStretch) const;
    uint64_t  GetSegmentEnd         (size_t keyframe, uint64_t newestEnd) const;

private:
    const KeyframeStore  & m_keyframes;
};
