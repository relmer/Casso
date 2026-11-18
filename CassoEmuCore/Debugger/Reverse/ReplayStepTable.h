#pragma once

#include "Pch.h"

#include "Debugger/Reverse/Replayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayStepTable
//
//  The registers and bytes of every instruction of one stretch of history,
//  from one replay of the stretch: what a step back and the trace pane read
//  behind live. Built for the stretch from start to end; a landing outside
//  it discards it.
//
////////////////////////////////////////////////////////////////////////////////

class ReplayStepTable
{
public:
    void  Reserve (size_t count) { m_steps.reserve (count); }
    void  Release ();
    void  Discard ();

    //  The table to fill from a replay of the stretch, then Commit for it.
    std::vector<ReplayStep>  & BeginFill();
    HRESULT                    Commit   (uint64_t start, uint64_t end);

    bool   IsBuiltFor (uint64_t start, uint64_t end) const { return m_hasSteps && m_start == start && m_end == end; }
    bool   HasSteps   () const { return m_hasSteps; }

    //  Whether the instruction at position is in the table, and whether the
    //  ones that led to position are.
    bool   Contains (uint64_t position) const { return m_hasSteps && position >= m_start && position < m_end; }
    bool   Leads    (uint64_t position) const { return m_hasSteps && position > m_start && position <= m_end; }

    const ReplayStep  & Get       (uint64_t position) const { return m_steps[static_cast<size_t> (position - m_start)]; }
    void                GetRecent (uint64_t position, size_t count, std::vector<ReplayStep> & outSteps) const;

    //  How many tables were built, for the tests of when a step back
    //  replays.
    size_t  GetBuildCount() const { return m_builds; }

private:
    std::vector<ReplayStep>  m_steps;
    uint64_t                 m_start    = 0;
    uint64_t                 m_end      = 0;
    size_t                   m_builds   = 0;
    bool                     m_hasSteps = false;
};
