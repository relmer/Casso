#include "Pch.h"

#include "Ui/Debugger/DebugViewBuilder.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewBuilder
//
////////////////////////////////////////////////////////////////////////////////

DebugViewBuilder::DebugViewBuilder() :
    m_session (m_target, m_sink, RunState::Paused)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewBuilder::Build
//
//  The session takes the input's tables, its target the input's capture, and
//  the view builds the read-only panes from them. Behind live, the trace pane
//  shows the history replayed to reach the machine's position instead.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewSnapshot DebugViewBuilder::Build (const DebuggerViewState & view, const DebugViewInput & input)
{
    DebuggerViewSnapshot  snapshot;



    m_target.SetCapture (input.capture);
    m_session.InstallView (input.session);

    snapshot = view.BuildCaptured (m_session, input.isPaused, m_runner);

    if (input.historyTrace.has_value())
    {
        DebuggerViewState::ApplyHistoryTrace (m_session, *input.historyTrace, snapshot);
    }

    return snapshot;
}
