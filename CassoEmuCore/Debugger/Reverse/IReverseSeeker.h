#pragma once

#include "Pch.h"

#include "Debugger/Reverse/ReverseOutcome.h"

struct ReplayReport;





////////////////////////////////////////////////////////////////////////////////
//
//  IReverseSeeker
//
//  The moves the step-back commands make through history: out of live, to
//  a position or a cycle, to a position with an outcome, a cut where a
//  replay diverged, and the test of whether the user stopped a long search.
//
////////////////////////////////////////////////////////////////////////////////

class IReverseSeeker
{
public:
    virtual ~IReverseSeeker() = default;

    virtual HRESULT  LeaveLive        () = 0;
    virtual HRESULT  LandAt           (uint64_t position, ReverseOutcome outcome, ReverseResult & result) = 0;
    virtual HRESULT  SeekToPosition   (uint64_t position, ReverseResult & result) = 0;
    virtual HRESULT  SeekToCycle      (uint64_t cycle, ReverseResult & result) = 0;
    virtual HRESULT  HandleDivergence (const ReplayReport & report, ReverseResult & result) = 0;
    virtual bool     IsStopDue        (uint64_t from, uint64_t reached) = 0;
};
