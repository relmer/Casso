#include "Pch.h"

#include "Debugger/Reverse/ReverseStepTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Build
//
//  Replays the whole stretch once from its keyframe, keeping the PC and
//  stack pointer of every instruction in it. A replay that diverges empties
//  the table and sets report.isDiverged.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepTable::Build (
    Replayer      & replayer,
    size_t          keyframe,
    uint64_t        start,
    uint64_t        end,
    ReplayReport  & report)
{
    HRESULT       hr        = S_OK;
    ReplayTarget  target;
    size_t        stepCount = 0;



    Discard();

    hr = replayer.RestoreKeyframe (keyframe);
    CHR (hr);

    target.position = end;

    hr = replayer.RunTo (target, end, nullptr, report, &m_steps);
    CHR (hr);

    if (report.isDiverged)
    {
        Discard();
        BAIL_OUT_IF (true, S_OK);
    }

    stepCount = m_steps.size();
    CBRA (stepCount == end - start);

    m_start    = start;
    m_end      = end;
    m_hasSteps = true;

    m_builds++;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Discard
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStepTable::Discard()
{
    m_steps.clear();

    m_hasSteps = false;
    m_start    = 0;
    m_end      = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiscardOutside
//
//  A landing outside the stretch the table covers drops it.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStepTable::DiscardOutside (uint64_t position)
{
    if (m_hasSteps && (position < m_start || position >= m_end))
    {
        Discard();
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Release
//
//  Frees the table's capacity once history stops.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStepTable::Release()
{
    Discard();

    m_steps = std::vector<ReplayStep>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBefore
//
//  Up to count instructions before position, oldest first; none when the
//  table does not cover them.
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStepTable::GetBefore (
    uint64_t                   position,
    size_t                     count,
    std::vector<ReplayStep>  & outSteps) const
{
    size_t  last  = 0;
    size_t  first = 0;



    outSteps.clear();

    if (!IsBehind (position))
    {
        return;
    }

    last  = static_cast<size_t> (position - m_start);
    first = (last > count) ? last - count : 0;

    outSteps.assign (m_steps.begin() + static_cast<ptrdiff_t> (first), m_steps.begin() + static_cast<ptrdiff_t> (last));
}
