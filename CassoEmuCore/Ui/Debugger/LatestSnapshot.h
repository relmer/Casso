#pragma once

#include "Ui/Debugger/DebuggerViewSnapshot.h"
#include "Ui/Debugger/StopChanges.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LatestSnapshot
//
//  The snapshot the debugger window applied last, none before the first,
//  and the values that changed at the last stop. The window sets both; the
//  panes read them.
//
////////////////////////////////////////////////////////////////////////////////

class LatestSnapshot
{
public:
    void                          Set (std::shared_ptr<const DebuggerViewSnapshot> snapshot) { m_snapshot = std::move (snapshot); }
    const DebuggerViewSnapshot  * Get () const { return m_snapshot.get(); }

    //  The session's mode in the snapshot, which an action's echo is written
    //  in; AppleWin's before the first snapshot.
    CommandMode  GetMode () const { return (m_snapshot != nullptr) ? m_snapshot->mode : CommandMode::AppleWin; }

    //  Compares the snapshot's registers, watches and stack with the last
    //  stop's; and the result.
    void                 UpdateStopChanges ();
    const StopChanges  & GetStopChanges    () const { return m_stopChanges; }

private:
    std::shared_ptr<const DebuggerViewSnapshot>  m_snapshot;
    StopChanges                                  m_stopChanges;
};
