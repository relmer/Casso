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
    uint64_t  position = m_machine.GetPosition();
    bool      isLive   = m_isRecording && m_isLive;



    CBRA (cpu);
    CBREx (isLive, HRESULT_FROM_WIN32 (ERROR_INVALID_STATE));

    if (m_isEditPending)
    {
        m_isEditPending = false;

        hr = OnMachineChanged();
        CHR (hr);
    }

    if (m_isPaused && position > m_pauseStart)
    {
        hr = CaptureBoundary (true, m_pauseStart);
        CHR (hr);
    }

    hr = m_keyframes.Export (m_machine.GetInputJournal(), outSaved);
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
    InputJournal            & journal               = m_machine.GetInputJournal();
    const KeyframeSettings  & kept                  = settings.keyframes;
    bool                      isSameSettings        = false;
    bool                      isAtLiveEnd           = false;
    bool                      isNewestHere          = false;



    CBRA (cpu);

    isSameSettings = !saved.segments.empty()                       &&
                     saved.settings.intervalCycles == kept.intervalCycles &&
                     saved.settings.wholeEvery     == kept.wholeEvery     &&
                     saved.settings.longestGroup   == kept.longestGroup;
    CBREx (isSameSettings, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    isAtLiveEnd = cpu->GetHistoryCycles() == saved.segments[0].info.liveEndCycle;
    CBREx (isAtLiveEnd, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    m_keyframes.Configure (kept);

    hr = UseWorkQueue();
    CHR (hr);

    m_steps.reserve         (static_cast<size_t> (kept.intervalCycles / kMinInstructionCycles) + kStepSlack);
    m_stackPointers.reserve (static_cast<size_t> (kept.intervalCycles / kMinInstructionCycles) + kStepSlack);

    m_machine.SetPosition (saved.segments[0].info.liveEndPosition);

    hr = m_keyframes.Import (saved, journal);
    CHR (hr);

    m_machine.SetInputJournalOn  (true);
    m_machine.SetHistoryRecorder (this);

    m_machine.GetDiskStore().SetFlushHold      (false);
    m_machine.GetDiskStore().SetMediaRetention (true);

    m_isRecording = true;
    m_isLive      = true;

    isNewestHere = m_keyframes.GetInfo (m_keyframes.GetCount() - 1).cycle == cpu->GetHistoryCycles();

    if (!isNewestHere)
    {
        hr = CaptureBoundary (false, 0);
        CHR (hr);
    }

    BecomeLive();

Error:
    return hr;
}
