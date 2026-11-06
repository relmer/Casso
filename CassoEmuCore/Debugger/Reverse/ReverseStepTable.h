#pragma once

#include "Pch.h"

#include "Debugger/Reverse/Replayer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepTable
//
//  The PC and stack pointer each instruction of one stretch began with, for
//  step back. One replay of the stretch from its keyframe fills it, so later
//  steps in the stretch are lookups. A replay that diverges empties the
//  table, and the caller cuts history.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseStepTable
{
public:
    HRESULT             Build           (Replayer & replayer, size_t keyframe, uint64_t start, uint64_t end, ReplayReport & report);
    void                Discard         ();
    void                DiscardOutside  (uint64_t position);
    void                Reserve         (size_t count) { m_steps.reserve (count); }
    void                Release         ();

    bool                HasSteps        () const { return m_hasSteps; }
    bool                IsBuiltFor      (uint64_t start, uint64_t end) const { return m_hasSteps && m_start == start && m_end == end; }
    bool                IsBehind        (uint64_t position) const { return m_hasSteps && position > m_start && position <= m_end; }
    size_t              GetBuildCount   () const { return m_builds; }
    const ReplayStep  & GetAt           (uint64_t position) const { return m_steps[static_cast<size_t> (position - m_start)]; }
    void                GetBefore       (uint64_t position, size_t count, std::vector<ReplayStep> & outSteps) const;

private:
    std::vector<ReplayStep>  m_steps;              // one stretch's registers and bytes per position
    uint64_t                 m_start    = 0;
    uint64_t                 m_end      = 0;
    size_t                   m_builds   = 0;
    bool                     m_hasSteps = false;
};
