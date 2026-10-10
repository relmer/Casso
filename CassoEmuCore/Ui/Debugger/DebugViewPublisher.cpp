#include "Pch.h"

#include "Ui/Debugger/DebugViewPublisher.h"
#include "Core/IWorkQueue.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewPublisher
//
////////////////////////////////////////////////////////////////////////////////

DebugViewPublisher::DebugViewPublisher (const DebuggerViewState & view, std::mutex & viewLock, PublishFunction publish) :
    m_view     (view),
    m_viewLock (viewLock),
    m_publish  (std::move (publish))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewPublisher::Submit
//
//  Only the first input since the queue last ran queues work; any after it
//  replaces the one waiting, and the work builds whichever is newest when it
//  starts.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewPublisher::Submit (DebugViewInput input, DebuggerViewSnapshot live)
{
    HRESULT                       hr        = S_OK;
    bool                          isToQueue = false;
    std::unique_lock<std::mutex>  held (m_lock);



    m_pending = Pending { std::move (input), std::move (live) };
    isToQueue = !m_isQueued;
    m_isQueued = true;

    held.unlock();

    if (!isToQueue)
    {
        return;
    }

    CBR (m_queue != nullptr);

    hr = m_queue->Submit (RunJob, this);
    CHRA (hr);

Error:
    if (FAILED (hr))
    {
        BuildPending();
    }

    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewPublisher::RunJob
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewPublisher::RunJob (void * context)
{
    static_cast<DebugViewPublisher *> (context)->BuildPending();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewPublisher::BuildPending
//
//  Takes the input waiting, builds it under the view lock, merges in what the
//  machine's thread built, and publishes; then looks again, since another may
//  have come in while it built. Leaves m_isQueued clear only once nothing
//  waits, under the same lock Submit tests it under.
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewPublisher::BuildPending()
{
    std::optional<Pending>  taken;
    DebuggerViewSnapshot    snapshot;



    for (;;)
    {
        {
            std::lock_guard<std::mutex>  held (m_lock);



            taken = std::move (m_pending);
            m_pending.reset();

            if (!taken.has_value())
            {
                m_isQueued = false;
                return;
            }
        }

        {
            std::lock_guard<std::mutex>  held (m_viewLock);



            snapshot = m_builder.Build (m_view, taken->input);
        }

        MergeLive (taken->live, snapshot);
        m_publish (std::make_shared<const DebuggerViewSnapshot> (std::move (snapshot)));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewPublisher::MergeLive
//
////////////////////////////////////////////////////////////////////////////////

void DebugViewPublisher::MergeLive (const DebuggerViewSnapshot & live, DebuggerViewSnapshot & snapshot)
{
    snapshot.panels      = live.panels;
    snapshot.diagnostics = live.diagnostics;
    snapshot.heatMap     = live.heatMap;
    snapshot.history     = live.history;
}
