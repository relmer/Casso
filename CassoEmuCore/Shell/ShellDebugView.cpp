#include "Pch.h"

#include "Shell/ShellDebugView.h"

#include "Debugger/CpuManagerRunDriver.h"
#include "Debugger/DebugViewCapture.h"
#include "Debugger/DebuggerController.h"
#include "Shell/CpuManager.h"
#include "Shell/DebugEventLogs.h"
#include "Shell/DebugViewModel.h"
#include "Shell/DebugWindowMailbox.h"
#include "Shell/IShellDebugContext.h"
#include "Shell/MachineHost.h"
#include "Shell/ShellHistory.h"
#include "Ui/Debugger/DebugViewBuilder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugView::ShellDebugView
//
////////////////////////////////////////////////////////////////////////////////

ShellDebugView::ShellDebugView (
    MachineHost        & machine,
    CpuManager         & cpuManager,
    ShellHistory       & history,
    DebugEventLogs     & eventLogs,
    IShellDebugContext & context,
    DebugViewModel     & viewModel,
    DebugWindowMailbox & mailbox) :
    m_machine    (machine),
    m_cpuManager (cpuManager),
    m_history    (history),
    m_eventLogs  (eventLogs),
    m_context    (context),
    m_viewModel  (viewModel),
    m_mailbox    (mailbox),
    m_publisher  (viewModel.GetState(), viewModel.GetLock(),
                  [this] (std::shared_ptr<const DebuggerViewSnapshot> snapshot)
                  {
                      m_mailbox.PublishSnapshot (std::move (snapshot));
                  })
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugView::Publish
//
//  Once a frame while the machine runs, and at once after anything the window
//  did; DebuggerViewState::IsBuildDue indicates which. This thread only
//  gathers: the panes are built on the publisher's queue.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugView::Publish()
{
    //  While the window is closed and BRKUNINIT is set, how often the heat
    //  map is folded.
    static constexpr ULONGLONG  kHiddenHeatFoldMs = 1000;
    DebuggerController        * controller        = m_context.GetDebugController();
    CpuManagerRunDriver       * runDriver         = m_context.GetDebugRunDriver();
    ULONGLONG                   now               = m_clock();
    bool                        isDue             = false;
    bool                        isBreaking        = false;
    bool                        isTaken           = true;
    bool                        isStepping        = false;
    DebugViewInput              input;
    DebuggerViewSnapshot        live;



    if (controller == nullptr)
    {
        return;
    }

    //  The heat map records for its pane, which no one sees while the window
    //  is closed, and for BRKUNINIT, which needs it whatever is shown; then it
    //  is folded now and then, so its counts never pile up unfolded.
    if (!m_isWindowShown.load())
    {
        isBreaking = controller->GetSession().HasUnwrittenBreak();

        controller->GetSession().GetTarget().SetHeatMapOn (isBreaking);

        if (isBreaking && now - m_heatFoldedAt >= kHiddenHeatFoldMs)
        {
            (void) controller->GetSession().GetTarget().FoldHeatMap();
            m_heatFoldedAt = now;
        }

        return;
    }

    isTaken = m_mailbox.IsSnapshotTaken();

    //  A step or a trace run un-pauses the machine until it lands. A view
    //  built in between has a running machine's rows, and the PC inside the
    //  call a step over runs, and the view after it puts them back: a flash
    //  in the trace pane and the code views. Such a run gets no view for
    //  kStepHoldMs from the first pass during it, so a long one refreshes as
    //  a run does.
    isStepping = runDriver != nullptr && runDriver->IsStepping();

    if (isStepping && !m_wasStepping)
    {
        m_steppingSince = now;
    }

    m_wasStepping = isStepping;

    if (DebuggerViewState::IsStepHeld (isStepping, now - m_steppingSince))
    {
        return;
    }

    isDue = DebuggerViewState::IsBuildDue (m_viewModel.IsDirty(), m_cpuManager.IsPaused(), m_wasPausedAtBuild,
                                           isTaken, now, m_builtAt);

    //  A stopped machine waiting on the heat map's rebuild, or the call
    //  record's, builds again a frame apart, so what is rebuilt shows when
    //  it comes in.
    isDue = isDue || ((m_wasHeatRebuilding || m_wasCallRebuilding) && now - m_builtAt >= DebuggerViewState::kBuildIntervalMs);

    if (!isDue)
    {
        return;
    }

    //  The clock panel reports the speed, which the CPU manager paces and the
    //  machine does not know.
    m_machine.GetClockPanel().SetSpeedMode (m_cpuManager.GetSpeedMode());

    //  Anything the window did is shown at once, the heat map included.
    if (m_viewModel.IsDirty())
    {
        m_viewModel.GetState().GetHeatMap().MarkHeatMapReadDue();
    }

    //  The heat map follows the machine through history.
    m_history.SyncHeat (true);

    if (!m_buildQueue.IsCreated())
    {
        CreateQueue();
    }

    Gather (input, live);
    m_publisher.Submit (std::move (input), std::move (live));
    m_buildsSubmitted++;

    m_builtAt           = now;
    m_wasPausedAtBuild  = m_cpuManager.IsPaused();
    m_wasHeatRebuilding = controller->GetSession().GetTarget().IsHeatMapRebuilding();
    m_wasCallRebuilding = controller->GetCallHistory().IsRebuilding();
    m_viewModel.ClearDirty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugView::CreateQueue
//
//  The build queue and the pool its panes are built on, made on the first
//  build. Without a queue the publisher builds on this thread; without a pool
//  the panes of a build are built one after another.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugView::CreateQueue()
{
    static constexpr size_t  kBuildQueueCapacity = 2;
    static constexpr DWORD   kBuildThreads       = 3;
    HRESULT                  hr                  = S_OK;



    hr = m_buildQueue.Create (kBuildQueueCapacity, L"Casso debugger build");
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_publisher.SetQueue (m_buildQueue.IsCreated() ? &m_buildQueue : nullptr);

    hr = m_buildPool.Create (kBuildThreads, L"Casso debugger pane builder");
    IGNORE_RETURN_VALUE (hr, S_OK);

    m_publisher.SetRunner (m_buildPool.IsCreated() ? &m_buildPool : nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugView::Gather
//
//  Everything the next snapshot reads that only this thread may touch: the
//  device panels and the heat map, built now since they drive the live
//  machine; the history status and, behind live, the instructions replayed
//  to get there; the machine's capture; and the session's tables. The run
//  state is the CPU manager's, not the session's, so it is passed in: the
//  command bar gates its stepping entries on it, and the code pane's
//  annotations are built only when it is paused.
//
////////////////////////////////////////////////////////////////////////////////

void ShellDebugView::Gather (DebugViewInput & input, DebuggerViewSnapshot & live)
{
    DebuggerController        & controller = *m_context.GetDebugController();
    DebugSession              & session    = controller.GetSession();
    IDebugTarget              & target     = session.GetTarget();
    bool                        isPaused   = m_cpuManager.IsPaused();
    auto                        capture    = std::make_shared<DebugViewCapture>();
    uint64_t                    first      = 0;



    m_viewModel.GetState().BuildLive (session, live, isPaused);
    live.history         = m_history.GetStatus();
    live.dataBlocks      = session.GetDataBlocks().GetAll();
    live.stepFilters     = session.GetStepFilter().GetEntries();
    live.isProfiling     = controller.GetHandlers().GetExecution().IsProfiling();
    live.profileProgress = m_eventLogs.GetProfileHistory().GetProgress();

    //  The profile is the execution handlers', and a real in it is written by
    //  the ROM's routine, so it is read only while the machine is paused.
    if (isPaused)
    {
        live.profile          = ProfileData();
        live.profileByAddress = ProfileData();
        controller.GetHandlers().GetExecution().BuildProfile (session, ProfileList::Opcodes,   *live.profile);
        controller.GetHandlers().GetExecution().BuildProfile (session, ProfileList::Addresses, *live.profileByAddress);
    }

    first = DebuggerViewState::GetTraceWindowFirst (target.GetTraceSize(), m_viewModel.GetState().GetTraceTop(), DebuggerViewState::kTraceRows);

    DebugViewCapture::Take (target, CallRecord(), (size_t) first, m_viewModel.GetState().IsTraceShown() ? DebuggerViewState::kTraceRows : 0, *capture);
    session.TakeView (m_sessionView);

    input.capture  = std::move (capture);
    input.session  = m_sessionView;
    input.isPaused = isPaused;
}
