#pragma once

#include "Core/ParallelWorkPool.h"
#include "Core/ThreadPoolWorkQueue.h"
#include "Debugger/DebugSessionView.h"
#include "Ui/Debugger/DebugViewPublisher.h"

class CpuManager;
class DebugEventLogs;
class DebugViewModel;
class DebugWindowMailbox;
class IShellDebugContext;
class MachineHost;
class ShellHistory;
struct DebugViewInput;
struct DebuggerViewSnapshot;





////////////////////////////////////////////////////////////////////////////////
//
//  ShellDebugView
//
//  The views built for the debugger window. On the CPU thread, once a frame
//  while the machine runs and at once after anything the window did, it
//  gathers what only that thread may touch and submits it to the publisher,
//  which builds the panes off the CPU thread and posts the snapshot to the
//  mailbox. While the window is closed, nothing is built, and the heat map is
//  on only for BRKUNINIT.
//
////////////////////////////////////////////////////////////////////////////////

class ShellDebugView
{
public:
    ShellDebugView (MachineHost        & machine,
                    CpuManager         & cpuManager,
                    ShellHistory       & history,
                    DebugEventLogs     & eventLogs,
                    IShellDebugContext & context,
                    DebugViewModel     & viewModel,
                    DebugWindowMailbox & mailbox);

    ShellDebugView             (const ShellDebugView &) = delete;
    ShellDebugView & operator= (const ShellDebugView &) = delete;

    //  CPU thread, each service pass: a build when one is due.
    void  Publish ();

    //  Whether the window is showing, since its view is built only while it
    //  is. Any thread.
    void  SetWindowShown (bool isShown) { m_isWindowShown.store (isShown); }

    //  The milliseconds a build is timed by, which a test replaces with a
    //  clock it moves on rather than sleeping; and how many views have been
    //  sent to be built, so a test checks that a pass built nothing without
    //  waiting out a timeout.
    using Clock = std::function<uint64_t()>;
    void      SetClock           (Clock clock)     { m_clock = std::move (clock); }
    uint64_t  GetBuildsSubmitted () const noexcept { return m_buildsSubmitted; }

private:
    void  Gather      (DebugViewInput & input, DebuggerViewSnapshot & live);
    void  CreateQueue ();

    MachineHost             & m_machine;
    CpuManager              & m_cpuManager;
    ShellHistory            & m_history;
    DebugEventLogs          & m_eventLogs;
    IShellDebugContext      & m_context;
    DebugViewModel          & m_viewModel;
    DebugWindowMailbox      & m_mailbox;

    std::atomic<bool>         m_isWindowShown      { false };
    ULONGLONG                 m_builtAt            = 0;
    bool                      m_wasPausedAtBuild   = false;
    bool                      m_wasHeatRebuilding  = false;   // the last build's heat map awaited its rebuilt heat
    bool                      m_wasCallRebuilding  = false;   // the last build's call stack awaited its rebuilt record
    bool                      m_wasStepping        = false;   // a step or a trace run was under way at the last pass
    ULONGLONG                 m_steppingSince      = 0;       // the first pass during it
    ULONGLONG                 m_heatFoldedAt       = 0;
    Clock                     m_clock              = [] () -> uint64_t { return GetTickCount64(); };
    uint64_t                  m_buildsSubmitted    = 0;

    //  Each build takes the view state lock while it reads the view state, and
    //  so does every change the window makes to it on the CPU thread; the
    //  device panels and the heat map, which drive the live machine, are built
    //  on the CPU thread from members that builds do not read. The session view
    //  persists between frames so the symbols and the debug file are shared
    //  until they change. The queue is declared last so it is destroyed first,
    //  which blocks until a running build finishes.
    DebugSessionView          m_sessionView;
    DebugViewPublisher        m_publisher;
    ParallelWorkPool          m_buildPool;
    ThreadPoolWorkQueue       m_buildQueue;
};
