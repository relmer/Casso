#include "Pch.h"

#include "Debugger/Reverse/ReplayStepTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayStepTable::Release
//
//  Discards the table and frees its memory.
//
////////////////////////////////////////////////////////////////////////////////

void ReplayStepTable::Release()
{
    Discard();

    m_steps = std::vector<ReplayStep>();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayStepTable::Discard
//
////////////////////////////////////////////////////////////////////////////////

void ReplayStepTable::Discard()
{
    m_steps.clear();

    m_hasSteps = false;
    m_start    = 0;
    m_end      = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayStepTable::BeginFill
//
////////////////////////////////////////////////////////////////////////////////

std::vector<ReplayStep> & ReplayStepTable::BeginFill()
{
    Discard();

    return m_steps;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayStepTable::Commit
//
//  The replay filled a step for every instruction of the stretch.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReplayStepTable::Commit (uint64_t start, uint64_t end)
{
    HRESULT  hr        = S_OK;
    size_t   stepCount = m_steps.size();



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
//  ReplayStepTable::GetRecent
//
//  Up to count instructions before position, oldest first; none when the
//  table doesn't have the ones that led to position.
//
////////////////////////////////////////////////////////////////////////////////

void ReplayStepTable::GetRecent (
    uint64_t                   position,
    size_t                     count,
    std::vector<ReplayStep>  & outSteps) const
{
    size_t  last  = 0;
    size_t  first = 0;



    outSteps.clear();

    if (!Leads (position))
    {
        return;
    }

    last  = static_cast<size_t> (position - m_start);
    first = (last > count) ? last - count : 0;

    outSteps.assign (m_steps.begin() + static_cast<ptrdiff_t> (first), m_steps.begin() + static_cast<ptrdiff_t> (last));
}
