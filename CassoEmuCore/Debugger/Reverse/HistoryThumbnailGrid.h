#pragma once

#include "Pch.h"

class KeyframeStore;





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailCell
//
//  One point on a history strip: the keyframe it is drawn from, by its
//  position and cycle, the host's clock when it was taken, and whether it is
//  the live end.
//
////////////////////////////////////////////////////////////////////////////////

struct HistoryThumbnailCell
{
    uint64_t  position = 0;
    uint64_t  cycle    = 0;
    uint64_t  wallTime = 0;
    bool      isLive   = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailGrid
//
//  Where a history strip's points lie: the oldest keyframe first, the live
//  end last, and between them a grid fixed to absolute positions, every
//  multiple of a step chosen from the span of history and the cell count.
//  Along the strip, every point is a cycle: the cells' points by their
//  cycles, a straight line between one and the next.
//
////////////////////////////////////////////////////////////////////////////////

class HistoryThumbnailGrid
{
public:
    //  The points for `count` cells over the keyframes, on the step in use,
    //  which is chosen afresh when it no longer suits; and the step dropped,
    //  for a history begun again.
    void              Plan              (const KeyframeStore & keyframes, int count, std::vector<HistoryThumbnailCell> & outCells) { PlanCells (keyframes, count, m_step, outCells); }
    void              Reset             () { m_step = 0; }

    static void       PlanCells         (const KeyframeStore & keyframes, int count, uint64_t & ioStep, std::vector<HistoryThumbnailCell> & outCells);
    static uint64_t   ChooseStep        (uint64_t span, int count, uint64_t step);
    static uint64_t   GetGridTop        (uint64_t newest, uint64_t step);
    static bool       DoesGridFit       (const KeyframeStore & keyframes, int count, uint64_t step, uint64_t top);
    static bool       TryFindAtOrBefore (const KeyframeStore & keyframes, uint64_t position, size_t & outIndex);

    //  The cell marked as the machine's position.
    static int        FindPlayheadCell  (const std::vector<HistoryThumbnailCell> & cells, uint64_t position, bool isBehindLive);

    //  The playhead line's place along the strip, in cells from the
    //  leading edge, for the machine at cycle with history ending at endCycle,
    //  or -1 with no cells; and the cycle at such an offset.
    static float      GetPlayheadOffset (const std::vector<HistoryThumbnailCell> & cells, uint64_t cycle, uint64_t endCycle);
    static uint64_t   GetCycleAtOffset  (const std::vector<HistoryThumbnailCell> & cells, float offset, uint64_t endCycle);

private:
    uint64_t  m_step = 0;
};
