#pragma once

#include "Pch.h"

#include "Debugger/Reverse/CallerLink.h"
#include "Debugger/Reverse/ReverseController.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepSearch
//
//  Where a step back over and a step back out land, for the controller
//  that owns it. The search replays history one stretch at a time and keeps
//  only each instruction's stack pointer. A step back out takes its target
//  from the debugger's call record first, when one is attached and has the
//  call: the chain of links read live is kept while the step backs walk it,
//  outermost first.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseStepSearch
{
public:
    explicit ReverseStepSearch (ReverseController & owner) : m_owner (owner) {}

    void      SetCallerProbe      (ReverseController::CallerProbe probe)           { m_callerProbe = std::move (probe); }
    void      SetCallerLinksProbe (ReverseController::CallerLinksProbe probe)      { m_callerLinksProbe = std::move (probe); }
    void      ReserveStackLevels  (size_t count)                                   { m_stackPointers.reserve (count); }
    void      ReleaseStackLevels  ()                                               { m_stackPointers = std::vector<Byte>(); }

    HRESULT   FindStepOverTarget  (uint64_t current, int spNow, bool & outFound, uint64_t & outTarget, bool & outIsGap);
    HRESULT   FindStepOutTarget   (uint64_t current, int spNow, bool & outFound, uint64_t & outTarget, bool & outIsGap);
    bool      HasNoCaller         () const;
    HRESULT   StepOutByRecord     (bool & outIsDone, ReverseResult & result);
    void      DiscardCallerLinks  ();

private:
    HRESULT   ReplayStackLevels   (uint64_t end, ReverseController::Stretch & outStretch, bool & outIsInGap);
    bool      TryGetCallerLinks   (size_t & outCount);
    bool      HasGapSince         (uint64_t cycle, uint64_t position) const;
    bool      IsAtCallerLink      (const CallerLink & link);

    ReverseController                   & m_owner;
    ReverseController::CallerProbe        m_callerProbe;            // the debugger's call record, when one is attached
    ReverseController::CallerLinksProbe   m_callerLinksProbe;       // the same record's calls still entered
    std::vector<CallerLink>               m_callerLinks;            // the chain step back out last read live, outermost first
    size_t                                m_callerDepth   = 0;      // the links outside the call step back out last landed on
    uint64_t                              m_callerLanding = UINT64_MAX;  // where it landed; any other position uses no link
    std::vector<Byte>                     m_stackPointers;          // a search's stack pointer per position, for one stretch
};
