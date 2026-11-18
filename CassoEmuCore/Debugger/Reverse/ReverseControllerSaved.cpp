#include "Pch.h"

#include "Debugger/Reverse/ReverseController.h"

#include "Debugger/Reverse/InputJournal.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseController::SaveHistory
//
//  As when leaving live, a debugger edit made here becomes a boundary
//  keyframe first, and a stretch run while recording was paused gets a
//  keyframe after its gap, so the history saved extends to the machine's
//  position.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::SaveHistory (SavedKeyframes & outSaved)
{
    HRESULT   hr       = S_OK;
    EmuCpu  * cpu      = m_machine.GetCpu();
    uint64_t  position = m_machine.GetRunner().GetPosition();
    bool      isLive   = m_span.IsRecording() && m_span.IsLive();



    CBRA (cpu);
    CBREx (isLive, HRESULT_FROM_WIN32 (ERROR_INVALID_STATE));

    if (m_span.TakeEdit())
    {
        hr = m_seeker.OnMachineChanged();
        CHR (hr);
    }

    if (m_span.IsPausedBefore (position))
    {
        hr = m_capturer.CaptureBoundary (true, m_span.GetPauseStart());
        CHR (hr);
    }

    hr = m_keyframes.Export (m_machine.GetHostInput().GetJournal(), outSaved);
    CHR (hr);

    outSaved.segments[0].info.liveEndPosition = position;
    outSaved.segments[0].info.liveEndCycle    = cpu->GetHistoryCycles();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseController::StartFromSaved
//
//  As Start, with the saved keyframes and input journal entries in place
//  of a first keyframe. The saved history must have been taken with these
//  settings but the budget, and the machine must be at its live end's
//  history cycle; its position is set to the live end's. The machine was
//  loaded from a state file rather than run there, so a boundary keyframe
//  is taken at the load, unless the newest saved keyframe is there already.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT ReverseController::StartFromSaved (
    const ReverseSettings  & settings,
    const SavedKeyframes   & saved)
{
    constexpr uint64_t        kMinInstructionCycles = 2;
    constexpr size_t          kStepSlack            = 64;
    HRESULT                   hr                    = S_OK;
    EmuCpu                  * cpu                   = m_machine.GetCpu();
    InputJournal            & journal               = m_machine.GetHostInput().GetJournal();
    const KeyframeSettings  & kept                  = settings.keyframes;
    size_t                    stepRoom              = static_cast<size_t> (kept.intervalCycles / kMinInstructionCycles) + kStepSlack;
    bool                      isSameSettings        = false;
    bool                      isAtLiveEnd           = false;
    bool                      isNewestHere          = false;



    CBRA (cpu);

    isSameSettings = !saved.segments.empty()                              &&
                     saved.settings.intervalCycles == kept.intervalCycles &&
                     saved.settings.wholeEvery     == kept.wholeEvery     &&
                     saved.settings.longestGroup   == kept.longestGroup;
    CBREx (isSameSettings, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isAtLiveEnd = cpu->GetHistoryCycles() == saved.segments[0].info.liveEndCycle;
    CBREx (isAtLiveEnd, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_keyframes.Configure (kept);

    hr = m_capturer.UseWorkQueue();
    CHR (hr);

    m_stepTable.Reserve (stepRoom);
    m_stepper.Reserve   (stepRoom);

    m_machine.GetRunner().SetPosition (saved.segments[0].info.liveEndPosition);

    hr = m_keyframes.Import (saved, journal);
    CHR (hr);

    m_machine.GetHostInput().SetJournalOn (true);
    m_machine.GetRunner().SetHistoryRecorder (this);

    m_machine.GetDiskStore().SetFlushHold      (false);
    m_machine.GetDiskStore().SetMediaRetention (true);

    m_span.Start();

    isNewestHere = m_keyframes.GetArena().GetInfo (m_keyframes.GetArena().GetCount() - 1).cycle == cpu->GetHistoryCycles();

    if (!isNewestHere)
    {
        hr = m_capturer.CaptureBoundary (false, 0);
        CHR (hr);
    }

    m_seeker.BecomeLive();

Error:
    return hr;
}
