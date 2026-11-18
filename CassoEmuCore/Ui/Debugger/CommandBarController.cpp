#include "Pch.h"

#include "Ui/Debugger/CommandBarController.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerCommands.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"
#include "Ui/Debugger/RunActions.h"
#include "Ui/Debugger/SessionActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarController::CommandBarController
//
////////////////////////////////////////////////////////////////////////////////

CommandBarController::CommandBarController (ICommandBarHost & host) :
    m_host (host)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarController::~CommandBarController
//
////////////////////////////////////////////////////////////////////////////////

CommandBarController::~CommandBarController() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarController::CreateEntries
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiToolbar::Entry> CommandBarController::CreateEntries()
{
    DebuggerCommands::Handlers  handlers;



    handlers.dispatch  = [this] (int id) { Run (id); };
    handlers.isEnabled = [this] (int id) { return IsEnabled (id); };
    handlers.isChecked = [this] (int id) { return id == DebuggerCommands::kTrace && m_host.GetCommandBarSnapshot() != nullptr && m_host.GetCommandBarSnapshot()->trace.isOn; };
    handlers.isDark    = [this]          { return m_host.IsDarkTheme(); };

    m_commands = std::make_unique<DebuggerCommands> (std::move (handlers));
    return m_commands->BuildEntries();
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarController::ApplyKeyScheme
//
////////////////////////////////////////////////////////////////////////////////

void CommandBarController::ApplyKeyScheme (DebuggerKeyScheme scheme)
{
    if (m_commands != nullptr)
    {
        m_commands->ApplyKeyScheme (scheme);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarController::Run
//
//  A click on the strip runs what the same key would: the key schemes'
//  actions go through the mapped-command path, and the bar's entries are
//  handled here.
//
////////////////////////////////////////////////////////////////////////////////

void CommandBarController::Run (int id)
{
    //  Pause while there is something to stop, Run otherwise.
    if (id == DebuggerCommands::kRunOrPause)
    {
        id = IsEnabled (DebuggerCommands::kPause) ? DebuggerCommands::kPause : DebuggerCommands::kRun;

        if (!IsEnabled (id))
        {
            return;
        }
    }

    if (id == DebuggerCommands::kShowNext)
    {
        m_host.ShowNextStatement();
        return;
    }

    if (id == DebuggerCommands::kTrace)
    {
        if (IsEnabled (id))
        {
            m_host.RunAction (SessionActions::GetTraceToggle (m_host.GetCommandBarSnapshot() != nullptr && m_host.GetCommandBarSnapshot()->trace.isOn, m_host.GetMode()));
        }

        return;
    }

    if (id == DebuggerCommands::kReverseContinue || id == DebuggerCommands::kGoLive)
    {
        m_host.RunAction (RunActions::GetReverse ((id == DebuggerCommands::kGoLive) ? DebugVerb::GoLive : DebugVerb::ReverseGo, m_host.GetMode()));
        return;
    }

    if (id == DebuggerCommands::kLoadFile)
    {
        m_host.LoadFileIntoMemory();
        return;
    }

    (void) m_host.RunMappedCommand (id);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarController::IsEnabled
//
////////////////////////////////////////////////////////////////////////////////

bool CommandBarController::IsEnabled (int id) const
{
    const DebuggerViewSnapshot  * snapshot       = m_host.GetCommandBarSnapshot();
    bool                          isPaused       = snapshot != nullptr && snapshot->isPaused;
    bool                          hasSelectedRow = id == DebuggerCommands::kRunToCursor && isPaused && m_host.HasSelectedCodeRow();



    return IsEnabled (id, snapshot, m_host.IsReplayingHistory(), hasSelectedRow);
}





////////////////////////////////////////////////////////////////////////////////
//
//  CommandBarController::IsEnabled
//
//  A running machine has no state to step through, so everything that acts
//  on a stopped one is off while it runs, and Pause is off while it is
//  already stopped. Run to Cursor needs a line to run to as well. The
//  choices that only change what the window shows stay live throughout.
//
////////////////////////////////////////////////////////////////////////////////

bool CommandBarController::IsEnabled (int id, const DebuggerViewSnapshot * snapshot, bool isReplaying, bool hasSelectedRow)
{
    bool  paused = snapshot != nullptr && snapshot->isPaused;



    //  While history recording is on, the trace is rebuilt from history, so
    //  there is nothing to turn on or off.
    if (id == DebuggerCommands::kTrace)
    {
        return snapshot == nullptr || !snapshot->history.isRecording;
    }

    if (id == DebuggerCommands::kRun)
    {
        return paused;
    }

    if (id == DebuggerCommands::kRunOrPause)
    {
        return IsEnabled (DebuggerCommands::kRun, snapshot, isReplaying, hasSelectedRow) || IsEnabled (DebuggerCommands::kPause, snapshot, isReplaying, hasSelectedRow);
    }

    //  A replay runs with the machine stopped, and Pause stops it.
    if (id == DebuggerCommands::kPause)
    {
        return !paused || isReplaying;
    }

    //  A load writes memory, which BLOAD does only with the machine stopped.
    if (id == DebuggerCommands::kStepInto || id == DebuggerCommands::kStepOver ||
        id == DebuggerCommands::kStepOut  || id == DebuggerCommands::kShowNext ||
        id == DebuggerCommands::kRunFrame || id == DebuggerCommands::kLoadFile)
    {
        return paused;
    }

    if (id == DebuggerCommands::kRunToCursor)
    {
        return paused && hasSelectedRow;
    }

    //  Moving through history needs a stopped machine and a recording, and
    //  going live needs the machine to be behind live.
    if (id == DebuggerCommands::kStepBackInto || id == DebuggerCommands::kStepBackOver ||
        id == DebuggerCommands::kStepBackOut  || id == DebuggerCommands::kReverseContinue)
    {
        return paused && snapshot->history.isRecording;
    }

    if (id == DebuggerCommands::kGoLive)
    {
        return paused && snapshot->history.isBehindLive;
    }

    return true;
}
