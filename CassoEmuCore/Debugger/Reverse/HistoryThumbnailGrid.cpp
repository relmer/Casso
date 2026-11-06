#include "Pch.h"

#include "Debugger/Reverse/HistoryThumbnailGrid.h"
#include "Debugger/Reverse/KeyframeStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailGrid::PlanCells
//
//  The first cell is the oldest keyframe held and the last the live end,
//  the newest, so the strip always runs from where history begins to where
//  it ends. The cells between are the grid points below the live end, one
//  step apart and ending at the last multiple of the step before it, each
//  the keyframe at or before it; a point older than history shows the
//  oldest keyframe. Because the grid is fixed to absolute positions, a point
//  keeps its keyframe until that keyframe is dropped. A step whose grid
//  reaches back past the second keyframe is chosen afresh.
//
////////////////////////////////////////////////////////////////////////////////

void HistoryThumbnailGrid::PlanCells (
    const KeyframeStore                & keyframes,
    int                                  count,
    uint64_t                           & ioStep,
    std::vector<HistoryThumbnailCell>  & outCells)
{
    size_t    held   = keyframes.GetCount();
    uint64_t  oldest = 0;
    uint64_t  newest = 0;
    uint64_t  top    = 0;
    uint64_t  back   = 0;
    size_t    index  = 0;
    int       i      = 0;



    outCells.clear();

    if (held == 0 || count <= 0)
    {
        return;
    }

    oldest = keyframes.GetInfo (0).position;
    newest = keyframes.GetInfo (held - 1).position;
    ioStep = ChooseStep (newest - oldest, count, ioStep);
    top    = GetGridTop (newest, ioStep);

    //  A step kept from a narrower strip or a longer history can reach back
    //  past the oldest keyframe, which would put the first grid points on
    //  the oldest keyframe beside the first cell; the step is chosen afresh
    //  instead, so every cell has a point of its own.
    if (!DoesGridFit (keyframes, count, ioStep, top))
    {
        ioStep = ChooseStep (newest - oldest, count, 0);
        top    = GetGridTop (newest, ioStep);
    }

    outCells.resize ((size_t) count);

    for (i = 0; i < count - 1; i++)
    {
        back = (uint64_t) (count - 2 - i) * ioStep;

        if (i == 0 || back > top || !TryFindAtOrBefore (keyframes, top - back, index))
        {
            index = 0;
        }

        outCells[(size_t) i].position = keyframes.GetInfo (index).position;
        outCells[(size_t) i].cycle    = keyframes.GetInfo (index).cycle;
        outCells[(size_t) i].wallTime = keyframes.GetInfo (index).wallTime;
    }

    outCells.back().position = newest;
    outCells.back().cycle    = keyframes.GetInfo (held - 1).cycle;
    outCells.back().wallTime = keyframes.GetInfo (held - 1).wallTime;
    outCells.back().isLive   = true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailGrid::ChooseStep
//
//  The step that spreads `count` cells over `span`: the grid cells cover
//  count - 1 steps. The step in use is kept while the span stays within a
//  quarter of the one it was chosen for, so the small swings of a full
//  history -- a group dropped at the old end, keyframes added at the new --
//  never move the grid; past that it is chosen afresh.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryThumbnailGrid::ChooseStep (
    uint64_t  span,
    int       count,
    uint64_t  step)
{
    constexpr uint64_t  kLow   = 3;
    constexpr uint64_t  kHigh  = 5;
    constexpr uint64_t  kParts = 4;



    uint64_t  ideal = (std::max) ((uint64_t) 1, span / (uint64_t) (std::max) (count - 1, 1));



    if (step == 0 || ideal * kParts < step * kLow || ideal * kParts >= step * kHigh)
    {
        return ideal;
    }

    return step;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailGrid::GetGridTop
//
//  The last multiple of the step before the live end, where the grid's
//  newest point lies.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryThumbnailGrid::GetGridTop (
    uint64_t  newest,
    uint64_t  step)
{
    return (newest > 0) ? (newest - 1) / step * step : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailGrid::DoesGridFit
//
//  Whether the oldest grid point, the second cell's, lies at or past the
//  second keyframe held, so it is not the oldest keyframe the first cell
//  already shows. With fewer than three cells there are no grid points, and
//  with one keyframe every cell shows it.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnailGrid::DoesGridFit (
    const KeyframeStore  & keyframes,
    int                    count,
    uint64_t               step,
    uint64_t               top)
{
    constexpr int     kOffGrid  = 2;    // the first cell and the live end are not on the grid
    constexpr size_t  kSecond   = 1;



    size_t    held = keyframes.GetCount();
    uint64_t  back = 0;



    if (count <= kOffGrid || held <= kSecond)
    {
        return true;
    }

    back = (uint64_t) (count - kOffGrid - 1) * step;

    return back <= top && top - back >= keyframes.GetInfo (kSecond).position;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailGrid::TryFindAtOrBefore
//
//  The newest keyframe taken at or before `position`; positions rise with
//  the index.
//
////////////////////////////////////////////////////////////////////////////////

bool HistoryThumbnailGrid::TryFindAtOrBefore (
    const KeyframeStore  & keyframes,
    uint64_t               position,
    size_t               & outIndex)
{
    size_t  low  = 0;
    size_t  high = keyframes.GetCount();
    size_t  mid  = 0;



    outIndex = 0;

    if (high == 0 || keyframes.GetInfo (0).position > position)
    {
        return false;
    }

    while (high - low > 1)
    {
        mid = low + (high - low) / 2;

        if (keyframes.GetInfo (mid).position <= position)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }

    outIndex = low;
    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailGrid::FindPlayheadCell
//
//  Live, the live end. Behind live, the last cell whose point is at or
//  before where the machine stands, which is the oldest when it stands
//  before them all.
//
////////////////////////////////////////////////////////////////////////////////

int HistoryThumbnailGrid::FindPlayheadCell (
    const std::vector<HistoryThumbnailCell>  & cells,
    uint64_t                                   position,
    bool                                       isBehindLive)
{
    int  marked = 0;
    int  i      = 0;



    if (cells.empty())
    {
        return -1;
    }

    if (!isBehindLive)
    {
        return (int) cells.size() - 1;
    }

    for (i = 0; i < (int) cells.size(); i++)
    {
        if (cells[(size_t) i].position <= position)
        {
            marked = i;
        }
    }

    return marked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailGrid::GetPlayheadOffset
//
////////////////////////////////////////////////////////////////////////////////

float HistoryThumbnailGrid::GetPlayheadOffset (
    const std::vector<HistoryThumbnailCell>  & cells,
    uint64_t                                   cycle,
    uint64_t                                   endCycle)
{
    size_t    count = cells.size();
    size_t    i     = 0;
    uint64_t  from  = 0;
    uint64_t  to    = 0;



    if (count == 0)
    {
        return -1.0f;
    }

    if (cycle <= cells[0].cycle)
    {
        return 0.0f;
    }

    while (i + 1 < count && cells[i + 1].cycle <= cycle)
    {
        i++;
    }

    from = cells[i].cycle;
    to   = (i + 1 < count) ? cells[i + 1].cycle : endCycle;

    if (to <= from)
    {
        return (float) i;
    }

    return (float) i + (float) (std::min (cycle, to) - from) / (float) (to - from);
}





////////////////////////////////////////////////////////////////////////////////
//
//  HistoryThumbnailGrid::GetCycleAtOffset
//
////////////////////////////////////////////////////////////////////////////////

uint64_t HistoryThumbnailGrid::GetCycleAtOffset (
    const std::vector<HistoryThumbnailCell>  & cells,
    float                                      offset,
    uint64_t                                   endCycle)
{
    size_t    count = cells.size();
    size_t    i     = 0;
    float     part  = 0.0f;
    uint64_t  from  = 0;
    uint64_t  to    = 0;



    if (count == 0)
    {
        return 0;
    }

    if (offset <= 0.0f)
    {
        return cells[0].cycle;
    }

    if (offset >= (float) count)
    {
        return std::max (endCycle, cells[count - 1].cycle);
    }

    i    = (size_t) offset;
    part = offset - (float) i;
    from = cells[i].cycle;
    to   = (i + 1 < count) ? cells[i + 1].cycle : endCycle;

    if (to <= from)
    {
        return from;
    }

    return from + (uint64_t) std::llround ((double) part * (double) (to - from));
}
