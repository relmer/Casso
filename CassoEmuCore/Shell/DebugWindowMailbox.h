#pragma once

#include "Debugger/Events/DebugLogDelta.h"
#include "Ui/Debugger/DebuggerActions.h"

struct DebuggerViewSnapshot;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugWindowMailbox
//
//  What crosses between the CPU thread and the debugger window's UI thread,
//  under one mutex: the last snapshot built and whether the window has taken
//  it, the console lines and event log deltas waiting for the window, and
//  the actions the window queued for the CPU thread.
//
////////////////////////////////////////////////////////////////////////////////

class DebugWindowMailbox
{
public:
    //  The snapshot when it is new, and the console lines since the last
    //  take; false with neither.
    bool                         TakeUpdate       (std::shared_ptr<const DebuggerViewSnapshot> & snapshot,
                                                   std::vector<std::string>                     & consoleLines);
    void                         PublishSnapshot  (std::shared_ptr<const DebuggerViewSnapshot> snapshot);
    bool                         IsSnapshotTaken  ();

    void                         AddConsoleLine   (const std::string & line);
    void                         AddConsoleLines  (const std::vector<std::string> & lines);

    void                         QueueAction      (const DebuggerAction & action, bool isEchoed);
    std::vector<DebuggerAction>  TakeActions      ();

    void                         AddEventLogs     (DebugLogDeltas & deltas);
    void                         TakeEventLogs    (DebugLogDeltas & deltas);
    void                         ClearEventLogs   ();

private:
    std::mutex                                     m_mutex;
    std::shared_ptr<const DebuggerViewSnapshot>    m_snapshot;
    bool                                           m_isSnapshotFresh = false;
    std::vector<std::string>                       m_consolePending;
    DebugLogDeltas                                 m_eventLogsPending;
    std::vector<DebuggerAction>                    m_actionsPending;
};
