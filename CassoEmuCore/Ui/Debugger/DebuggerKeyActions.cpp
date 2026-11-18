#include "Pch.h"

#include "Ui/Debugger/DebuggerKeyActions.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/BreakpointActions.h"
#include "Ui/Debugger/RunActions.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeyActions::GetForKey
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerAction> DebuggerKeyActions::GetForKey (
    DebuggerKeySchemes::Action   action,
    const DebuggerViewSnapshot * snapshot,
    int                          selectedRow,
    CommandMode                  mode)
{
    using Action = DebuggerKeySchemes::Action;

    std::optional<DebuggerAction>  taken;
    std::optional<Word>            selected;
    std::optional<Word>            current;



    if (snapshot != nullptr && selectedRow >= 0 && selectedRow < (int) snapshot->code.size())
    {
        selected = snapshot->code[(size_t) selectedRow].address;
    }

    if (snapshot != nullptr)
    {
        current = selected.has_value() ? selected : std::optional<Word> (snapshot->pc);
    }

    switch (action)
    {
    case Action::Run:      taken = RunActions::GetRun      (mode); break;
    case Action::StepInto: taken = RunActions::GetStepInto (mode); break;
    case Action::StepOver: taken = RunActions::GetStepOver (mode); break;
    case Action::StepOut:  taken = RunActions::GetStepOut  (mode); break;
    case Action::RunFrame: taken = RunActions::GetRunFrame();      break;

    case Action::StepBackInto: taken = RunActions::GetReverse (DebugVerb::StepBack,     mode); break;
    case Action::StepBackOver: taken = RunActions::GetReverse (DebugVerb::StepBackOver, mode); break;
    case Action::StepBackOut:  taken = RunActions::GetReverse (DebugVerb::StepBackOut,  mode); break;
    case Action::ReverseRun:   taken = RunActions::GetReverse (DebugVerb::ReverseGo,    mode); break;

    case Action::RunToCursor:
        if (selected.has_value())
        {
            taken = RunActions::GetRunToCursor (*selected);
        }

        break;

    case Action::ToggleBreakpoint:
        if (snapshot != nullptr && current.has_value())
        {
            taken = BreakpointActions::GetToggleBreakpoint (*snapshot, *current, mode);
        }

        break;

    case Action::Pause:
    default:
        break;
    }

    if (snapshot != nullptr && !snapshot->isPaused && IsStepAction (action))
    {
        taken.reset();
    }

    return taken;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeyActions::IsStepAction
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerKeyActions::IsStepAction (DebuggerKeySchemes::Action action)
{
    using Action = DebuggerKeySchemes::Action;



    switch (action)
    {
    case Action::StepInto:
    case Action::StepOver:
    case Action::StepOut:
    case Action::RunToCursor:
    case Action::RunFrame:
    case Action::StepBackInto:
    case Action::StepBackOver:
    case Action::StepBackOut:
    case Action::ReverseRun:
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeyActions::IsRunAction
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerKeyActions::IsRunAction (DebuggerKeySchemes::Action action)
{
    using Action = DebuggerKeySchemes::Action;



    return action == Action::Run || action == Action::ReverseRun || action == Action::RunToCursor;
}
