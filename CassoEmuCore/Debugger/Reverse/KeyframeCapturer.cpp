#include "Pch.h"

#include "Debugger/Reverse/KeyframeCapturer.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/Reverse/HistorySpan.h"
#include "Debugger/Reverse/IHistoryObserver.h"
#include "Debugger/Reverse/InputJournal.h"
#include "Debugger/Reverse/KeyframeLayout.h"
#include "Debugger/Reverse/KeyframeStore.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCapturer::KeyframeCapturer
//
////////////////////////////////////////////////////////////////////////////////

KeyframeCapturer::KeyframeCapturer (
    MachineHost              & machine,
    KeyframeStore            & keyframes,
    HistorySpan              & span,
    ThreadPoolWorkQueue      & workQueue,
    std::array<uint64_t, 4>  & pagesRun) :
    m_machine   (machine),
    m_keyframes (keyframes),
    m_span      (span),
    m_workQueue (workQueue),
    m_pagesRun  (pagesRun)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCapturer::SetKeyframeListener
//
//  The owner's listener replaces any it set before; a null one removes it.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeCapturer::SetKeyframeListener (
    const void        * owner,
    KeyframeListener    listener)
{
    std::erase_if (m_keyframeListeners, [owner] (const std::pair<const void *, KeyframeListener> & each) { return each.first == owner; });

    if (listener)
    {
        m_keyframeListeners.emplace_back (owner, std::move (listener));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCapturer::UseWorkQueue
//
//  Keyframes are packed on the queue a test set, or on the thread pool,
//  whose queue is created on first use with room for every buffer the store
//  can have in flight.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeCapturer::UseWorkQueue()
{
    HRESULT  hr        = S_OK;
    bool     isCreated = m_workQueue.IsCreated();



    if (m_workQueueOverride != nullptr)
    {
        m_keyframes.SetWorkQueue (m_workQueueOverride);
        BAIL_OUT_IF (true, S_OK);
    }

    if (!isCreated)
    {
        hr = m_workQueue.Create (KeyframeLayout::kJobCount, L"Casso history keyframes");
        CHR (hr);
    }

    m_keyframes.SetWorkQueue (&m_workQueue);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCapturer::CaptureNow
//
//  Saves the machine once and passes the state to the keyframe store.
//
//  The save shares the RAM chunks and disk tracks not written since the
//  last one, so only what changed is copied; the keyframe store flattens it
//  into a separate buffer and packs it on its work queue. Every buffer, list
//  and writer here is reused, so a capture doesn't allocate. The history
//  observer's side bytes, if any, are stored with it. The keyframe listeners
//  are called after, unless the caller marks the keyframe first and calls
//  them itself.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeCapturer::CaptureNow (bool isNotifying)
{
    HRESULT            hr         = S_OK;
    EmuCpu           * cpu        = m_machine.GetCpu();
    InputJournal     & journal    = m_machine.GetHostInput().GetJournal();
    uint64_t           position   = m_machine.GetRunner().GetPosition();
    uint64_t           cycle      = 0;
    size_t             journalEnd = 0;
    size_t             count      = 0;
    bool               isFirst    = false;
    std::string_view   hostState;



    CBRA (cpu);

    cycle = cpu->GetHistoryCycles();
    count = m_keyframes.GetArena().GetCount();

    // The store orders keyframes by cycle, which a power cycle restarts. A
    // capture before the cycle count passes the newest keyframe's again (a
    // debugger edit or a disk change soon after a power cycle) cannot follow
    // it, so history before the power cycle goes and starts over here.
    if (count > 0 && m_keyframes.GetArena().GetInfo (count - 1).cycle >= cycle)
    {
        m_keyframes.Clear();
    }

    // A host write no read has taken yet is in the snapshot; the sync record
    // puts it in front of any replay that crosses this point as well.
    m_hostWriter.Reuse (m_hostWriter.TakeBytes());

    hr = m_machine.GetHostInput().SaveState (m_hostWriter);
    CHR (hr);

    hostState = std::string_view (reinterpret_cast<const char *> (m_hostWriter.GetBytes().data()), m_hostWriter.GetBytes().size());

    m_machine.GetHostInput().Record (InputKind::HostState, 0, 0, hostState);

    journalEnd = journal.GetEndIndex();

    m_writer.Reuse      (m_writer.TakeBytes(), m_writer.TakeSegments());
    m_writer.SetSharing (true);

    hr = m_machine.GetStateStream().SaveState (m_writer);
    CHR (hr);

    m_machine.GetStateStream().CheckSharedSave (m_writer);

    m_side.clear();

    if (m_observer != nullptr)
    {
        m_observer->OnKeyframeAdding (position, m_side);
    }

    isFirst = m_keyframes.GetArena().GetCount() == 0;

    hr = m_keyframes.Add (position, cycle, journalEnd, m_writer, m_side);
    CHR (hr);

    NoteActivity (isFirst);

    journal.DiscardBefore (m_keyframes.GetArena().GetInfo (0).journalIndex);

    PruneRetainedMedia();

    if (isNotifying)
    {
        NotifyKeyframe (position);
    }

Error:
    if (m_onCaptured)
    {
        m_onCaptured();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCapturer::CaptureBoundary
//
//  A keyframe a replay loads rather than runs to: after a change from
//  outside the recorded inputs, or where recording resumed after a gap that
//  began at gapStart. While paused, recording stops again just after it.
//  The keyframe listeners are called once it is marked.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT KeyframeCapturer::CaptureBoundary (
    bool      isAfterGap,
    uint64_t  gapStart)
{
    HRESULT  hr = S_OK;



    hr = CaptureNow (false);
    CHR (hr);

    if (isAfterGap)
    {
        m_keyframes.MarkNewestGap (gapStart);
    }
    else
    {
        m_keyframes.MarkNewestBoundary();
    }

    NotifyKeyframe (m_machine.GetRunner().GetPosition());

    m_span.NotePause (m_machine.GetRunner().GetPosition());

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCapturer::NoteActivity
//
//  The keyframe just added gets what the run did since the one before:
//  the pages instructions started on, collected by HistoryRecorder, and
//  the Disk II controller's switch count and motor. The pages start over
//  for the next stretch. Pages the machine ran while in history, behind
//  live, are counted in too, which only makes the stretch replayed more
//  often.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeCapturer::NoteActivity (bool isFirst)
{
    Disk2Controller  * controller = m_machine.GetRefs().diskController;
    StretchActivity    activity;



    activity.pagesRun       = m_pagesRun;
    activity.isKnown        = !isFirst;
    activity.diskSwitches   = (controller != nullptr) ? controller->GetCard().GetSwitchAccessCount() : 0;
    activity.isDiskSpinning = controller != nullptr && controller->GetLines().IsMotorOn();

    m_keyframes.SetNewestActivity (activity);
    m_pagesRun = {};
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCapturer::NotifyKeyframe
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeCapturer::NotifyKeyframe (uint64_t position)
{
    for (const auto & [owner, listener] : m_keyframeListeners)
    {
        listener (position);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCapturer::PruneRetainedMedia
//
//  The disk store frees the disks that left their bays before every
//  keyframe stored.
//
////////////////////////////////////////////////////////////////////////////////

void KeyframeCapturer::PruneRetainedMedia()
{
    if (m_keyframes.GetArena().GetCount() > 0)
    {
        m_machine.GetDiskStore().PruneRetainedMedia (m_keyframes.GetArena().GetInfo (0).position);
    }
}
