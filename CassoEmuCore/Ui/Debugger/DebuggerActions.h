#pragma once

#include "Debugger/BreakpointDefinition.h"
#include "Debugger/DebugCommand.h"
#include "Ui/Debugger/DebuggerKeySchemes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerAction
//
//  What a control in the debugger window does, as the command the session
//  runs directly, with no command text in between (FR-135). `echo` is the
//  equivalent command the console shows behind the prompt of `echoMode`, or
//  of the session's mode when that is empty. It is display only: nothing
//  parses it.
//
////////////////////////////////////////////////////////////////////////////////

struct DebuggerAction;

struct BreakpointStep
{
    //  Actions: the actions run as one undo step. Import: the breakpoint
    //  lines of the file at `path`, as one step. Undo and Redo take the
    //  newest step back or forward.
    enum class Kind
    {
        Actions,
        Import,
        Undo,
        Redo,
    };

    Kind                         kind = Kind::Actions;
    std::vector<DebuggerAction>  actions;
    std::string                  path;
};

struct DebuggerAction
{
    DebugCommand                   command;
    std::string                    echo;
    std::optional<CommandMode>     echoMode;

    //  What the user typed for the command's value, evaluated in the
    //  session's mode when the action runs: into `a1` for AddWatch and
    //  SetRegister, and as the values for EnterBytes and EnterWords.
    std::string                    operand;

    //  A watch's value edit: the operand is exactly one value, and for
    //  EnterBytes one that fits a byte, so the edit writes only the watched
    //  bytes, which are what its undo puts back.
    bool                           isOneValue = false;

    //  A breakpoint as the window's controls define it, evaluated into the
    //  command when the action runs: the command itself, or the new
    //  definition of the breakpoint an EditBreakpoint command edits.
    std::optional<BreakpointDefinition>  breakpoint;

    //  A breakpoints pane action, which runs in place of `command` and goes
    //  on the pane's undo list (FR-120).
    std::optional<BreakpointStep>  breakpointStep;

    //  The window putting back what it had open, not a choice the user
    //  made: the command runs and the console has neither echo nor reply.
    bool                           quiet = false;

    //  Whether the console lists the echo and the reply. Without it the
    //  console has only an error, or the reply of an action whose reply is
    //  its result.
    bool                           isEchoed = false;

    //  The reply is what the action produces, as a search's matches or a
    //  timing are, so the console lists it whether or not it is echoed.
    bool                           isResult = false;

    //  For a memory window's undo or redo: the bytes memory must contain from
    //  the command's address, or nothing is written.
    std::vector<Byte>              required;

    //  Where the window displays the action's error or result: over the pane
    //  it came from, or, for an action a dialog ran, in a message box.
    std::wstring                   replyPane;
    bool                           isReplyBoxed = false;
};

//  Builds an action in the session's mode, for a pane that does not know it.
using DebuggerActionBuilder = std::function<DebuggerAction (CommandMode mode)>;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerActions
//
//  The actions the window's controls, keys and menus take are built by the
//  action families named below. Each builds the command the matching typed
//  line parses to, and its echo in the session's mode, `mode`.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerActions
{
public:
    //  The action for a verb, echoed as the AppleWin line in the words of the
    //  mode. RunActions, BreakpointActions, WatchActions, MemoryActions,
    //  SymbolActions and SessionActions build every action through here.
    static DebuggerAction  Make (DebugVerb verb, const std::string & appleWinLine, CommandMode mode);
};