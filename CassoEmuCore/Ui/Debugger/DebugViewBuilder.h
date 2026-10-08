#pragma once

#include "Debugger/CapturedDebugTarget.h"
#include "Debugger/DebugSession.h"
#include "Debugger/DebugSessionView.h"
#include "Debugger/DebugViewCapture.h"
#include "Debugger/IDebugNotificationSink.h"
#include "Ui/Debugger/DebuggerViewState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewInput
//
//  Everything one build of the panes reads, taken on the machine's thread:
//  the machine's capture, the session's tables, the run state, and, while
//  the machine is behind live, the instructions history replayed to reach it.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugViewInput
{
    std::shared_ptr<const DebugViewCapture>   capture;
    DebugSessionView                          session;
    bool                                      isPaused = true;
    std::optional<std::vector<TraceRecord>>   historyTrace;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugViewBuilder
//
//  Builds the panes that only read, from an input, with a session of its own
//  over a captured target: nothing it does touches the machine or the live
//  session, so it runs on any thread. One build at a time; the panes within a
//  build run at once on the runner it is given.
//
////////////////////////////////////////////////////////////////////////////////

class DebugViewBuilder
{
public:
                           DebugViewBuilder ();

    DebugViewBuilder             (const DebugViewBuilder &) = delete;
    DebugViewBuilder & operator= (const DebugViewBuilder &) = delete;

    void                   SetRunner        (IParallelRunner * runner) { m_runner = runner; }
    DebuggerViewSnapshot   Build            (const DebuggerViewState & view, const DebugViewInput & input);

private:
    CapturedDebugTarget        m_target;
    NullDebugNotificationSink  m_sink;
    DebugSession               m_session;
    IParallelRunner          * m_runner = nullptr;
};
