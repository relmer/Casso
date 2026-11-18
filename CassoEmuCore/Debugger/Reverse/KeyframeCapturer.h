#pragma once

#include "Pch.h"

#include "Core/StateWriter.h"

class HistorySpan;
class IHistoryObserver;
class IWorkQueue;
class KeyframeStore;
class MachineHost;
class ThreadPoolWorkQueue;





////////////////////////////////////////////////////////////////////////////////
//
//  KeyframeCapturer
//
//  Takes reverse execution's keyframes: the machine's state, the host input
//  state with it, the history observer's side bytes, and what the run did
//  in the stretch before it. The journal entries of keyframes dropped over
//  budget go with them, and the disk store frees the disks taken out of
//  their bays before the oldest keyframe. Each keyframe goes to the keyframe
//  listeners.
//
////////////////////////////////////////////////////////////////////////////////

class KeyframeCapturer
{
public:
    //  Called with the position of each keyframe taken.
    using KeyframeListener = std::function<void (uint64_t position)>;

    KeyframeCapturer (MachineHost              & machine,
                      KeyframeStore            & keyframes,
                      HistorySpan              & span,
                      ThreadPoolWorkQueue      & workQueue,
                      std::array<uint64_t, 4>  & pagesRun);

    KeyframeCapturer (const KeyframeCapturer &)             = delete;
    KeyframeCapturer & operator= (const KeyframeCapturer &) = delete;

    void                SetObserver         (IHistoryObserver * observer) { m_observer = observer; }
    IHistoryObserver  * GetObserver         () const                      { return m_observer; }
    void                SetKeyframeListener (const void * owner, KeyframeListener listener);

    //  The queue a test packs on in place of the thread pool's.
    void     SetWorkQueue (IWorkQueue * queue) { m_workQueueOverride = queue; }
    HRESULT  UseWorkQueue ();

    //  Called after each capture, failed or not, since the next one falls
    //  due from it.
    void     SetCapturedCallback (std::function<void ()> callback) { m_onCaptured = std::move (callback); }

    HRESULT  CaptureNow      (bool isNotifying = true);
    HRESULT  CaptureBoundary (bool isAfterGap, uint64_t gapStart);

private:
    void  NoteActivity       (bool isFirst);
    void  NotifyKeyframe     (uint64_t position);
    void  PruneRetainedMedia ();

    MachineHost                & m_machine;
    KeyframeStore              & m_keyframes;
    HistorySpan                & m_span;
    ThreadPoolWorkQueue        & m_workQueue;
    std::array<uint64_t, 4>    & m_pagesRun;
    StateWriter                  m_writer;       // reused, so the capacity of its section and segment lists is reused
    StateWriter                  m_hostWriter;   // the host input state taken with every capture
    std::vector<Byte>            m_side;         // the observer's bytes for the keyframe being taken
    IHistoryObserver           * m_observer          = nullptr;
    IWorkQueue                 * m_workQueueOverride = nullptr;
    std::function<void ()>       m_onCaptured;

    //  Each owner's keyframe listener, in the order they were set.
    std::vector<std::pair<const void *, KeyframeListener>>  m_keyframeListeners;
};
