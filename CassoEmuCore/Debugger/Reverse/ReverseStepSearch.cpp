#include "Pch.h"

#include "Debugger/Reverse/ReverseStepSearch.h"

#include "Debugger/DebugMemoryView.h"
#include "Debugger/Reverse/IReverseStopTest.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayStackLevels
//
//  The stack pointer each instruction from the start of the stretch that
//  holds end - 1 up to end began with, into m_stackPointers: a replay from
//  the stretch's keyframe that keeps one byte per instruction and nothing
//  else. A position recording skipped is in a gap, and outStretch.end is
//  where the gap began.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepSearch::ReplayStackLevels (
    uint64_t    end,
    ReverseController::Stretch   & outStretch,
    bool      & outIsInGap)
{
    HRESULT       hr      = S_OK;
    bool          isFound = false;
    size_t        count   = 0;
    ReplayTarget  target;
    ReplayReport  report;



    outIsInGap = false;

    hr = m_owner.LeaveLive();
    CHR (hr);

    hr = m_owner.m_keyframes.WaitForPending();
    CHR (hr);

    isFound = m_owner.TryFindStretch (end - 1, outStretch);
    CBRAEx (isFound, E_INVALIDARG);

    outIsInGap = end - 1 >= outStretch.end;
    BAIL_OUT_IF (outIsInGap, S_OK);

    m_stackPointers.clear();

    hr = m_owner.m_replayer.RestoreKeyframe (outStretch.keyframe);
    CHR (hr);

    target.position = end;

    hr = m_owner.m_replayer.RunTo (target, outStretch.end, nullptr, report, nullptr, &m_stackPointers);
    CHR (hr);

    if (report.isDiverged)
    {
        hr = m_owner.HandleDivergence (report, m_owner.m_cutResult);
        CHR (hr);

        m_owner.m_isCut = true;
        BAIL_OUT_IF (true, S_OK);
    }

    count = m_stackPointers.size();
    CBRA (count == end - outStretch.start);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindStepOverTarget
//
//  The instruction before current, unless it was a return: an instruction
//  that began with the stack two or more bytes deeper than now. Then the
//  target is the newest instruction before it that began no deeper than now,
//  which is the JSR (or the interrupted instruction) the return came back
//  over. A gap stops the search at its edge. The search replays one stretch
//  at a time, keeping only the stack pointers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepSearch::FindStepOverTarget (
    uint64_t    current,
    int         spNow,
    bool      & outFound,
    uint64_t  & outTarget,
    bool      & outIsGap)
{
    constexpr int               kReturnDepth = 2;
    HRESULT                     hr           = S_OK;
    uint64_t                    oldest       = m_owner.GetOldestPosition();
    uint64_t                    q            = 0;
    bool                        isReturn     = false;
    bool                        isInGap      = false;
    ReverseController::Stretch  stretch;



    outFound = false;
    outIsGap = false;

    BAIL_OUT_IF (current <= oldest, S_OK);

    hr = ReplayStackLevels (current, stretch, isInGap);
    CHR (hr);

    BAIL_OUT_IF (m_owner.m_isCut, S_OK);

    isReturn = !isInGap && spNow - static_cast<int> (m_stackPointers.back()) >= kReturnDepth;

    if (!isReturn)
    {
        outFound  = true;
        outIsGap  = isInGap;
        outTarget = isInGap ? stretch.end : current - 1;
        BAIL_OUT_IF (true, S_OK);
    }

    for (q = current - 1; q > oldest && !outFound; q--)
    {
        // The stretch is used up: on to the one before it.
        if (q == stretch.start)
        {
            if (m_owner.IsStopDue (current, q))
            {
                outFound    = true;
                outTarget   = q;
                m_owner.m_isStopped = true;
                break;
            }

            hr = ReplayStackLevels (q, stretch, isInGap);
            CHR (hr);

            BAIL_OUT_IF (m_owner.m_isCut, S_OK);
        }

        outFound  = isInGap || static_cast<int> (m_stackPointers[static_cast<size_t> (q - 1 - stretch.start)]) >= spNow;
        outIsGap  = isInGap;
        outTarget = isInGap ? q : q - 1;
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindStepOutTarget
//
//  Walking back, the call that entered the current routine is the newest
//  JSR (the stack two bytes deeper after it) or interrupt (three bytes)
//  whose stack after it is as shallow as anything since: a call that already
//  returned left a shallower stack behind it. A gap stops the search at its
//  edge. The search replays one stretch at a time, newest first, keeping
//  only the stack pointers.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepSearch::FindStepOutTarget (
    uint64_t    current,
    int         spNow,
    bool      & outFound,
    uint64_t  & outTarget,
    bool      & outIsGap)
{
    constexpr int               kCallDepth      = 2;
    constexpr int               kInterruptDepth = 3;
    HRESULT                     hr              = S_OK;
    uint64_t                    oldest          = m_owner.GetOldestPosition();
    int                         spNext          = spNow;
    int                         ceiling         = spNow;
    int                         sp              = 0;
    int                         pushed          = 0;
    uint64_t                    q               = current;
    bool                        isInGap         = false;
    bool                        isStopDue       = false;
    ReverseController::Stretch  stretch;



    outFound = false;
    outIsGap = false;

    while (q > oldest && !outFound)
    {
        if (m_owner.IsStopDue (current, q))
        {
            outFound    = true;
            outTarget   = q;
            m_owner.m_isStopped = true;
            break;
        }

        hr = ReplayStackLevels (q, stretch, isInGap);
        CHR (hr);

        BAIL_OUT_IF (m_owner.m_isCut, S_OK);

        if (isInGap)
        {
            outFound  = true;
            outIsGap  = true;
            outTarget = (q == current) ? stretch.end : q;
            break;
        }

        for (; q > stretch.start && !outFound; q--)
        {
            sp     = m_stackPointers[static_cast<size_t> (q - 1 - stretch.start)];
            pushed = sp - spNext;

            if ((pushed == kCallDepth || pushed == kInterruptDepth) && spNext >= ceiling)
            {
                outFound  = true;
                outTarget = q - 1;
            }

            spNext  = sp;
            ceiling = std::max (ceiling, spNext);
        }
    }

    // The progress reaches where the search ended, the last stretch included.
    isStopDue = m_owner.IsStopDue (current, q);
    IGNORE_RETURN_VALUE (isStopDue, false);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasNoCaller
//
//  Only live: the call record follows the machine as it runs, and stands
//  for where history ends, not for a position inside it.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStepSearch::HasNoCaller() const
{
    bool  isAskable = m_owner.m_isRecording && m_owner.m_isLive && m_callerProbe && m_owner.m_keyframes.GetCount() > 0;



    return isAskable && m_callerProbe (m_owner.m_keyframes.GetInfo (0).cycle);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StepOutByRecord
//
//  The innermost link the call record gives is the call the search would
//  find, so the machine seeks straight to the cycle it began at: one
//  keyframe loaded and at most one stretch replayed. The landing is checked
//  against the link (the cycle, PC and stack pointer, and the JSR or BRK
//  there), and on a mismatch, with no link, with a link older than history,
//  or with a gap since the call, outIsDone stays false and the search runs.
//  A replay that diverges cuts history, and result then reports the cut.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseStepSearch::StepOutByRecord (
    bool           & outIsDone,
    ReverseResult  & result)
{
    HRESULT     hr       = S_OK;
    size_t      count    = 0;
    bool        hasLinks = false;
    bool        isUsable = false;
    CallerLink  link;



    outIsDone = false;

    hr = m_owner.m_keyframes.WaitForPending();
    CHR (hr);

    hasLinks = TryGetCallerLinks (count);
    BAIL_OUT_IF (!hasLinks || count == 0, S_OK);

    link     = m_callerLinks[count - 1];
    isUsable = link.cycle >= m_owner.m_keyframes.GetInfo (0).cycle && !HasGapSince (link.cycle, m_owner.m_machine.GetPosition());

    if (!isUsable)
    {
        DiscardCallerLinks();
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_owner.SeekToCycle (link.cycle, result);
    CHR (hr);

    if (result.outcome == ReverseOutcome::HistoryCut)
    {
        outIsDone = true;
        BAIL_OUT_IF (true, S_OK);
    }

    if (!IsAtCallerLink (link))
    {
        DiscardCallerLinks();
        BAIL_OUT_IF (true, S_OK);
    }

    if (m_owner.m_hasSteps && (result.position < m_owner.m_stepsStart || result.position >= m_owner.m_stepsEnd))
    {
        m_owner.DiscardStepTable();
    }

    m_callerDepth   = count - 1;
    m_callerLanding = m_owner.m_machine.GetPosition();
    result.outcome  = ReverseOutcome::Moved;
    outIsDone       = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TryGetCallerLinks
//
//  How many of the links step back out may use from where the machine
//  stands: live, the record is read now and all of them; behind live, the
//  ones outside the call the last step back out landed on, while the
//  machine is still on it. From that call, the search would find the next
//  link out: any call between the two that the stack has not risen above
//  since is held by the record, as a link between them.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStepSearch::TryGetCallerLinks (size_t & outCount)
{
    bool  isAskable = m_owner.m_isRecording && m_owner.m_isLive && m_callerLinksProbe && m_owner.m_keyframes.GetCount() > 0;
    bool  isLanded  = !m_owner.m_isLive && m_owner.m_machine.GetPosition() == m_callerLanding;



    if (isAskable)
    {
        m_callerLinksProbe (m_callerLinks);

        m_callerDepth   = m_callerLinks.size();
        m_callerLanding = m_owner.m_machine.GetPosition();
    }

    outCount = m_callerDepth;

    return isAskable || isLanded;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasGapSince
//
//  Whether recording skipped any position between cycle and position: a
//  keyframe after cycle, at or before position, that ends a gap, or, live
//  while paused, the stretch the next step back would close.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStepSearch::HasGapSince (
    uint64_t  cycle,
    uint64_t  position) const
{
    size_t                index  = m_owner.m_keyframes.GetCount();
    bool                  hasGap = m_owner.m_isLive && m_owner.m_isPaused && position > m_owner.m_pauseStart;
    const KeyframeInfo  * info   = nullptr;



    while (index > 0 && !hasGap)
    {
        index--;
        info = &m_owner.m_keyframes.GetInfo (index);

        if (info->cycle <= cycle)
        {
            break;
        }

        hasGap = info->hasGapBefore && info->position <= position;
    }

    return hasGap;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsAtCallerLink
//
//  The machine stands where the link's call began: at its cycle, with its
//  PC and stack pointer, and on a JSR or BRK when it was one.
//
////////////////////////////////////////////////////////////////////////////////

bool ReverseStepSearch::IsAtCallerLink (const CallerLink & link)
{
    const EmuCpu     * cpu    = m_owner.m_machine.GetCpu();
    DebugMemoryView    memory (m_owner.m_machine);
    Byte               opcode = 0;
    bool               isRead = memory.TryPeek (link.callSite, opcode);
    bool               isCall = link.isInterrupt || (isRead && opcode == link.opcode);



    return cpu->GetHistoryCycles() == link.cycle && cpu->GetPC() == link.callSite && cpu->GetSP() == link.stackLevel && isCall;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiscardCallerLinks
//
////////////////////////////////////////////////////////////////////////////////

void ReverseStepSearch::DiscardCallerLinks()
{
    m_callerLinks.clear();

    m_callerDepth   = 0;
    m_callerLanding = UINT64_MAX;
}
