#pragma once

#include "Pch.h"

#include "Debugger/Reverse/CallerLink.h"
#include "Debugger/Reverse/HistoryTimeline.h"
#include "Debugger/Reverse/ReverseOutcome.h"

class HistorySpan;
class IReverseSeeker;
class KeyframeStore;
class MachineHost;
class Replayer;
class ReplayStepTable;
struct ReplayReport;
struct ReplayStep;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseStepper
//
//  The step-back commands: back one instruction, back over a call, back out
//  of a routine, and back by cycles. A step back reads its stretch's step
//  table, built the first time a step enters the stretch. A step back over
//  or out searches history one stretch at a time and stores only each
//  instruction's stack pointer. A step back out takes its target from the
//  debugger's call record first, when one is attached and has the call:
//  the chain of links read live is stored while the step backs walk it,
//  outermost first. A replay that diverges cuts history, and the command
//  then reports the cut.
//
////////////////////////////////////////////////////////////////////////////////

class ReverseStepper
{
public:
    //  Whether the debugger's call record has no call since
    //  historyStartCycle that entered the code running now.
    using CallerProbe = std::function<bool (uint64_t historyStartCycle)>;

    //  The record's calls entered and not yet returned from, outermost first.
    using CallerLinksProbe = std::function<void (std::vector<CallerLink> & outLinks)>;

    ReverseStepper (MachineHost      & machine,
                    KeyframeStore    & keyframes,
                    HistoryTimeline  & timeline,
                    Replayer         & replayer,
                    HistorySpan      & span,
                    ReplayStepTable  & stepTable,
                    IReverseSeeker   & seeker);

    ReverseStepper (const ReverseStepper &)             = delete;
    ReverseStepper & operator= (const ReverseStepper &) = delete;

    void  SetCallerProbe      (CallerProbe probe)      { m_callerProbe = std::move (probe); }
    void  SetCallerLinksProbe (CallerLinksProbe probe) { m_callerLinksProbe = std::move (probe); }

    //  The stack-pointer buffer, sized for a stretch of count instructions.
    void  Reserve (size_t count) { m_stackPointers.reserve (count); }
    void  Release ();

    void  DiscardCallerLinks();

    HRESULT  StepBack       (ReverseResult & result);
    HRESULT  StepBackOver   (ReverseResult & result);
    HRESULT  StepBackOut    (ReverseResult & result);
    HRESULT  StepBackCycles (uint64_t cycles, ReverseResult & result);

    //  Behind live, the step table with the instructions that led to the
    //  machine's position, for the trace pane; GetRecentSteps lists up to
    //  count of them, oldest first.
    HRESULT  PrepareRecentSteps (ReverseResult & result);
    void     GetRecentSteps     (size_t count, std::vector<ReplayStep> & outSteps) const;

private:
    using Stretch = HistoryTimeline::Stretch;

    HRESULT  GetStep            (uint64_t position, ReplayStep & outStep, bool & outIsInGap, uint64_t & outGapStart);
    HRESULT  BuildStepTable     (const Stretch & stretch);
    HRESULT  ReplayStackLevels  (uint64_t end, Stretch & outStretch, bool & outIsInGap);
    HRESULT  FindStepOverTarget (uint64_t current, int spNow, bool & outFound, uint64_t & outTarget, bool & outIsGap);
    HRESULT  FindStepOutTarget  (uint64_t current, int spNow, bool & outFound, uint64_t & outTarget, bool & outIsGap);
    bool     HasNoCaller        () const;
    HRESULT  StepOutByRecord    (bool & outIsDone, ReverseResult & result);
    bool     TryGetCallerLinks  (size_t & outCount);
    bool     HasGapSince        (uint64_t cycle, uint64_t position) const;
    bool     IsAtCallerLink     (const CallerLink & link);
    bool     TryFindStretch     (uint64_t position, Stretch & outStretch) const;
    uint64_t GetOldestPosition  () const;
    HRESULT  Cut                (const ReplayReport & report);
    HRESULT  Land               (bool isFound, uint64_t target, bool isGap, ReverseResult & result);

    MachineHost                & m_machine;
    KeyframeStore              & m_keyframes;
    HistoryTimeline            & m_timeline;
    Replayer                   & m_replayer;
    HistorySpan                & m_span;
    ReplayStepTable            & m_stepTable;
    IReverseSeeker             & m_seeker;

    CallerProbe                  m_callerProbe;                 // the debugger's call record, when one is attached
    CallerLinksProbe             m_callerLinksProbe;            // the same record's calls not yet returned from
    std::vector<CallerLink>      m_callerLinks;                 // the chain step back out last read live, outermost first
    size_t                       m_callerDepth   = 0;           // the links outside the call step back out last landed on
    uint64_t                     m_callerLanding = UINT64_MAX;  // where it landed; any other position doesn't use a link
    std::vector<Byte>            m_stackPointers;               // a search's stack pointer per position, for one stretch

    bool                         m_isCut         = false;       // a replay diverged and cut history; m_cutResult has where
    bool                         m_isStopped     = false;       // a search for a step's target was stopped short
    ReverseResult                m_cutResult;
};
