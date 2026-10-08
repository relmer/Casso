#pragma once

#include "Core/IWorkQueue.h"
#include "Ui/Debugger/DebugViewBuilder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewPublisher
//
//  Takes the inputs the machine's thread gathers and builds the debugger's
//  panes from them on a work queue, so the machine's thread only gathers.
//  An input submitted while an earlier one still waits replaces it: the
//  queue builds the newest and nothing piles up behind a slow build.
//
//  Each build holds the view lock while it reads the view state, which the
//  window's actions change on the machine's thread under the same lock. The
//  parts the machine's thread built itself, the device panels, the heat map
//  and the history status, are merged in, and the snapshot goes to publish.
//
//  Submit on the machine's thread; publish is called on the queue's thread.
//
////////////////////////////////////////////////////////////////////////////////

class DebugViewPublisher
{
public:
    using PublishFunction = std::function<void (std::shared_ptr<const DebuggerViewSnapshot>)>;

                DebugViewPublisher (const DebuggerViewState & view, std::mutex & viewLock, PublishFunction publish);

    DebugViewPublisher             (const DebugViewPublisher &) = delete;
    DebugViewPublisher & operator= (const DebugViewPublisher &) = delete;

    void        SetQueue          (IWorkQueue * queue)       { m_queue = queue; }
    void        SetRunner         (IParallelRunner * runner) { m_builder.SetRunner (runner); }

    //  The panes the machine's thread built (live) go out with the panes built
    //  from input. With no queue, or one that will not take the work, the
    //  build runs on the calling thread.
    void        Submit            (DebugViewInput input, DebuggerViewSnapshot live);

    //  Builds what waits until nothing does. The queue's work, or a test's.
    void        BuildPending      ();

private:
    struct Pending
    {
        DebugViewInput        input;
        DebuggerViewSnapshot  live;
    };

    static void  RunJob           (void * context);
    static void  MergeLive        (const DebuggerViewSnapshot & live, DebuggerViewSnapshot & snapshot);

    const DebuggerViewState  & m_view;
    std::mutex               & m_viewLock;
    PublishFunction            m_publish;
    IWorkQueue               * m_queue    = nullptr;
    DebugViewBuilder           m_builder;

    std::mutex                 m_lock;
    std::optional<Pending>     m_pending;
    bool                       m_isQueued = false;
};
