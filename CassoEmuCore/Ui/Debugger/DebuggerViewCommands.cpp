#include "Pch.h"

#include "Ui/Debugger/DebuggerViewCommands.h"
#include "Ui/Debugger/BreakpointImport.h"
#include "Ui/Debugger/CodeViews.h"
#include "Ui/Debugger/ConsoleLinks.h"
#include "Ui/Debugger/DebuggerLayout.h"
#include "Ui/Debugger/DevicePanelViews.h"
#include "Ui/Debugger/MemoryGoTo.h"
#include "Ui/Debugger/MemoryWindowViews.h"

#include "Debugger/CommandModeHelp.h"
#include "Debugger/DebugSession.h"
#include "Debugger/AppleWinArgumentForms.h"
#include "Debugger/AppleWinBreakpointParser.h"
#include "Debugger/AppleWinParser.h"
#include "Debugger/GSSquaredParser.h"
#include "Debugger/AppleWinCommandTable.h"
#include "Debugger/Handlers/ConfigHandlers.h"
#include "Config/IFileSystem.h"
#include "Core/TextEncoding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::DebuggerViewCommands
//
////////////////////////////////////////////////////////////////////////////////

DebuggerViewCommands::DebuggerViewCommands (
    CodeViews          & codeViews,
    MemoryWindowViews  & memoryWindows,
    DevicePanelViews   & panels) :
    m_codeViews     (codeViews),
    m_memoryWindows (memoryWindows),
    m_panels        (panels)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::AddRequests
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewCommands::AddRequests (DebuggerViewSnapshot & snapshot) const
{
    snapshot.goTo           = m_goTo;
    snapshot.showPane       = m_showPane;
    snapshot.showPaneSerial = m_showPaneSerial;

    snapshot.actionReply        = m_actionReply;
    snapshot.actionReplySerial  = m_actionReplySerial;
    snapshot.actionReplyPane    = m_actionReplyPane;
    snapshot.isActionReplyBoxed = m_isActionReplyBoxed;
    snapshot.isActionReplyError = m_isActionReplyError;

    snapshot.suggestion       = m_suggestion;
    snapshot.suggestionSerial = m_suggestionSerial;

    snapshot.canUndoBreakpoints  = m_breakpointHistory.CanUndo();
    snapshot.canRedoBreakpoints  = m_breakpointHistory.CanRedo();
    snapshot.undoBreakpointsText = m_breakpointHistory.GetUndoText();
    snapshot.redoBreakpointsText = m_breakpointHistory.GetRedoText();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ExecuteLine
//
//  A help listing's rows come back tabbed, for the console to lay out at
//  its width.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewCommands::ExecuteLine (DebugSession & session, const std::string & line, CommandMode mode)
{
    CommandModeHelp::TabbedRows  tabbed (true);
    Reply                        reply = session.ExecuteLine (line, mode);



    session.FormatReply (reply, mode);
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ExecuteConsoleLine
//
//  The echo takes the prompt of the mode the line was typed in, as batch
//  writes it, not the mode a MODE line leaves in force. A line sent with its
//  own mode runs in that mode alone and leaves the session's as it was.
//  The files the reply writes out are links in the console.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebuggerViewCommands::ExecuteConsoleLine (
    DebugSession                & session,
    const std::string           & line,
    std::optional<CommandMode>    lineMode)
{
    CommandMode               mode  = lineMode.value_or (session.GetMode());
    std::vector<std::string>  lines;
    Reply                     reply;



    reply = ExecuteWindowLine (session, line, mode);

    m_suggestion = reply.suggestion;
    ++m_suggestionSerial;

    ConsoleLinks::LinkFiles (reply.text, reply.files);

    lines.push_back (DebugSession::GetPrompt (mode) + line);
    lines.insert (lines.end(), reply.text.begin(), reply.text.end());

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::RunPendingScript
//
//  The rest of a script a run interrupted, once the machine has stopped:
//  its lines run through this window as the script's first lines did, and
//  their text is returned for the console. Empty with no script waiting.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebuggerViewCommands::RunPendingScript (DebugSession & session)
{
    DebugSession::ScriptLineRunner  previous = session.GetScriptLineRunner();
    DebugSession::PendingScript     script;
    Reply                           reply;



    if (!session.TakePendingScript (script))
    {
        return {};
    }

    session.SetScriptLineRunner ([this, &session] (const std::string & scriptLine, CommandMode scriptMode)
    {
        return ExecuteWindowLine (session, scriptLine, scriptMode);
    });

    ConfigHandlers::RunScript (session, script.content, script.mode, reply);
    session.SetScriptLineRunner (std::move (previous));

    ConsoleLinks::LinkFiles (reply.text, reply.files);

    return reply.text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::AddCommandGap
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewCommands::AddCommandGap (std::vector<std::string> & lines)
{
    if (!lines.empty())
    {
        lines.insert (lines.begin(), std::string (kCommandGap));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ExecuteAction
//
//  PANEL is the window's own, so it runs on the panes; anything else goes to
//  the session. A script the command starts runs its lines back through this
//  window, as one a typed line starts does.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebuggerViewCommands::ExecuteAction (DebugSession & session, const DebuggerAction & action)
{
    CommandMode               mode = action.echoMode.value_or (session.GetMode());
    std::vector<std::string>  lines;
    Reply                     reply;



    if (action.breakpointStep.has_value())
    {
        return ExecuteBreakpointStep (session, *action.breakpointStep, action.isEchoed);
    }

    reply = ExecuteActionCommand (session, action);

    if (action.quiet)
    {
        return lines;
    }

    reply.mode = mode;
    session.FormatReply (reply, mode);
    AddActionLines (session, action, reply, lines);

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::AddActionLines
//
//  An echoed action's prompt, echo and reply go in the console. An action
//  from the window that isn't echoed has no console line: its reply, when it
//  is an error or the action's result, goes in the snapshot for the window.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewCommands::AddActionLines (DebugSession & session, const DebuggerAction & action, const Reply & reply, std::vector<std::string> & lines)
{
    CommandMode  mode = action.echoMode.value_or (session.GetMode());



    if (action.isEchoed)
    {
        lines.push_back (DebugSession::GetPrompt (mode) + action.echo);

        for (const std::string & text : reply.text)
        {
            lines.push_back (ConsoleLinks::LinkFiles (text, reply.files));
        }

        return;
    }

    m_actionReply.clear();
    m_actionReplySerial++;
    m_actionReplyPane    = action.replyPane;
    m_isActionReplyBoxed = action.isReplyBoxed;
    m_isActionReplyError = reply.status != CommandStatus::Ok;

    if (action.isResult || reply.hasNotice || reply.status != CommandStatus::Ok)
    {
        m_actionReply = reply.text;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ExecuteActionCommand
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewCommands::ExecuteActionCommand (DebugSession & session, const DebuggerAction & action)
{
    DebugSession::ScriptLineRunner  previous = session.GetScriptLineRunner();
    DebugCommand                    command;
    std::string                     error;
    Reply                           reply;



    if (!TryResolveCommand (session, action, command, error))
    {
        reply.command = action.command.sourceName;
        reply.SetError (CommandStatus::Error, "invalid arguments", error);
        return reply;
    }

    if (command.verb == DebugVerb::ListPanels || command.verb == DebugVerb::OpenPanel || command.verb == DebugVerb::ClosePanel)
    {
        reply.command = command.sourceName;
        m_panels.RunPanelCommand (session, command, reply);
        return reply;
    }

    for (size_t i = 0; i < action.required.size(); i++)
    {
        Word  address = (Word) (command.a1 + i);
        Byte  value   = 0;

        if (session.GetTarget().TryPeek (address, value) && value != action.required[i])
        {
            reply.command = command.sourceName;
            reply.SetError (CommandStatus::Error, "memory changed", std::format ("${:04X} has changed since the edit, so nothing was written.", address));
            return reply;
        }
    }

    session.SetScriptLineRunner ([this, &session] (const std::string & scriptLine, CommandMode scriptMode)
    {
        return ExecuteWindowLine (session, scriptLine, scriptMode);
    });

    reply = session.Execute (command);
    session.SetScriptLineRunner (std::move (previous));
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::TryResolveCommand
//
//  A breakpoint's definition is evaluated into the command that sets it, or
//  into an edit's new definition, and a search's items into its bytes; no
//  command text is parsed. What the user typed is
//  evaluated in the session's mode: one address or register value, or the
//  values to write, each split at spaces as a typed deposit's are, a byte
//  where it fits and a word, low byte first, where it does not or where the
//  command writes words. A watch's value edit takes one value, and a byte
//  watch's one that fits a byte.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerViewCommands::TryResolveCommand (DebugSession & session, const DebuggerAction & action, DebugCommand & command, std::string & error)
{
    static constexpr Word  kMaxByte = 0xFF;
    DebugCommand           made;
    std::istringstream     tokens (action.operand);
    std::string            token;
    Word                   value    = 0;
    bool                   isWord   = false;



    command = action.command;

    if (action.breakpoint.has_value() && command.verb == DebugVerb::EditBreakpoint)
    {
        if (!AppleWinBreakpointParser::TryMakeBreakpoint (*action.breakpoint, session, made, error))
        {
            return false;
        }

        command.definition = std::make_shared<const DebugCommand> (made);
        return true;
    }

    if (action.breakpoint.has_value())
    {
        return AppleWinBreakpointParser::TryMakeBreakpoint (*action.breakpoint, session, command, error);
    }

    if (command.verb == DebugVerb::SearchMemory)
    {
        return AppleWinArgumentForms::TryParseSearchItems (action.operand, session, command, error);
    }

    if (action.operand.empty())
    {
        return true;
    }

    //  A watch's text goes to the session as typed, which reads it as a BASIC
    //  variable where it is one, and otherwise evaluates it.
    if (command.verb == DebugVerb::AddWatch)
    {
        command.text = action.operand;
        return true;
    }

    if (command.verb != DebugVerb::EnterBytes && command.verb != DebugVerb::EnterWords)
    {
        return AppleWinArgumentForms::TryEvaluate (action.operand, session, command.a1, error);
    }

    command.values.clear();
    command.mask.clear();

    while (tokens >> token)
    {
        if (!AppleWinArgumentForms::TryEvaluate (token, session, value, error))
        {
            return false;
        }

        if (action.isOneValue && !command.values.empty())
        {
            error = "Type one value.";
            return false;
        }

        if (action.isOneValue && command.verb == DebugVerb::EnterBytes && value > kMaxByte)
        {
            error = std::format ("${:X} does not fit in a byte.", value);
            return false;
        }

        isWord = command.verb == DebugVerb::EnterWords || value > kMaxByte;

        command.values.push_back ((Byte) value);
        command.mask.push_back ((Byte) kMaxByte);

        if (isWord)
        {
            command.values.push_back ((Byte) (value >> 8));
            command.mask.push_back ((Byte) kMaxByte);
        }
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ExecuteBreakpointStep
//
//  Each action runs directly and is echoed in its mode. An undo or redo runs
//  no command and gives one line. An import's lines are the file's text, so
//  they alone are parsed, each in the dialect it is written in.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> DebuggerViewCommands::ExecuteBreakpointStep (DebugSession & session, const BreakpointStep & step, bool isEchoed)
{
    std::vector<std::string>         lines;
    BreakpointHistory::ActionRunner  act;
    BreakpointImport::LineRunner     run;
    std::vector<std::string>         summary;



    act = [this, &session, &lines, isEchoed] (const DebuggerAction & action)
    {
        DebuggerAction  echoed = action;
        CommandMode     mode   = action.echoMode.value_or (session.GetMode());
        Reply           reply  = ExecuteActionCommand (session, action);

        echoed.isEchoed = isEchoed;
        reply.mode      = mode;
        session.FormatReply (reply, mode);
        AddActionLines (session, echoed, reply, lines);
        return reply;
    };

    run = [this, &session, &lines, isEchoed] (const std::string & line, CommandMode mode)
    {
        Reply  reply = ExecuteSessionLine (session, line, mode);

        if (isEchoed)
        {
            lines.push_back (DebugSession::GetPrompt (mode) + line);
        }

        if (isEchoed || reply.status != CommandStatus::Ok)
        {
            for (const std::string & text : reply.text)
            {
                lines.push_back (ConsoleLinks::LinkFiles (text, reply.files));
            }
        }

        return reply;
    };

    switch (step.kind)
    {
    case BreakpointStep::Kind::Undo:
        if (m_breakpointHistory.TryUndo (session, summary) && isEchoed)
        {
            lines.insert (lines.end(), summary.begin(), summary.end());
        }

        break;

    case BreakpointStep::Kind::Redo:
        if (m_breakpointHistory.TryRedo (session, summary) && isEchoed)
        {
            lines.insert (lines.end(), summary.begin(), summary.end());
        }

        break;

    case BreakpointStep::Kind::Import:
        m_breakpointHistory.Record (session, [this, &session, &step, &run, &lines] { ImportBreakpoints (session, step.path, run, lines); });
        break;

    default:
        m_breakpointHistory.Record (session, [&step, &act]
        {
            for (const DebuggerAction & action : step.actions)
            {
                act (action);
            }
        });
        break;
    }

    return lines;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ImportBreakpoints
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewCommands::ImportBreakpoints (
    DebugSession                         & session,
    const std::string                    & path,
    const BreakpointImport::LineRunner   & run,
    std::vector<std::string>             & lines)
{
    IFileSystem  * files   = session.GetHostFiles().GetFileSystem();
    std::string    script;
    std::string    line;
    int            skipped = 0;
    HRESULT        hr      = S_OK;



    CBRF (files != nullptr, lines.push_back ("This session cannot read or write host files."));

    hr = files->ReadAllText (session.GetHostFiles().ResolvePath (path), script);
    CHRF (hr, lines.push_back (std::format ("{} could not be read.", path)));

    skipped = BreakpointImport::Run (session, script, run);
    line    = std::format ("Imported the breakpoints{}; skipped {} line{} that {} not breakpoints.",
                           session.GetHostFiles().GetPathEcho (path, "from"), skipped, skipped == 1 ? "" : "s", skipped == 1 ? "is" : "are");
    lines.push_back (ConsoleLinks::LinkFiles (line, { TextEncoding::WideToNarrow (session.GetHostFiles().ResolvePath (path)) }));

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ExecuteWindowLine
//
//  THIS WINDOW SHOWS EVERY PANE AT ONCE, so AppleWin's commands that pick a
//  layout have nothing to change and say so. The screen views and appearance
//  commands are not available: the emulator window shows the screen, and the
//  theme sets the colors and font.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewCommands::ExecuteWindowLine (DebugSession & session, const std::string & line, CommandMode mode)
{
    std::string              text     = line;
    size_t                   first    = line.find_first_not_of (" \t");
    std::istringstream       stream;
    std::string              name;
    std::string              argument;
    const AppleWinCommand  * entry    = nullptr;
    Reply                    reply;
    GSSquaredParseResult     parsed;
    DebugVerb                verb     = DebugVerb::None;



    //  GSSquared and WinDbg have no layout commands, and their words are not
    //  AppleWin's. PANEL, which they reach the way they reach any Casso
    //  command -- by its bare name, or after WinDbg's `!` -- is still the
    //  window's, or the panels saved with the window could not reopen in
    //  those modes.
    if (mode == CommandMode::GSSquared || mode == CommandMode::WinDbg)
    {
        //  debug and nodebug are GSSquared's PANEL, which the window runs.
        if (mode == CommandMode::GSSquared && !session.GetInterpreter().IsAssembling())
        {
            parsed = GSSquaredParser::Parse (line, session);
            verb   = parsed.commands.size() == 1 ? parsed.commands.front().verb : DebugVerb::None;

            if (verb == DebugVerb::ListPanels || verb == DebugVerb::OpenPanel || verb == DebugVerb::ClosePanel)
            {
                reply.command = line;
                m_panels.RunPanelCommand (session, parsed.commands.front(), reply);
                session.FormatReply (reply, mode);
                return reply;
            }
        }

        if (mode == CommandMode::WinDbg)
        {
            if (first == std::string::npos || line[first] != '!')
            {
                return ExecuteSessionLine (session, line, mode);
            }

            text = line.substr (first + 1);
        }

        stream.str (text);
        stream >> name;

        entry = (name.empty() || session.GetInterpreter().IsAssembling()) ? nullptr : AppleWinCommandTable::Find (name);

        if (entry != nullptr && entry->verb == DebugVerb::ListPanels)
        {
            return ExecutePanelLine (session, text, line, mode);
        }

        return ExecuteSessionLine (session, line, mode);
    }

    //  In Monitor mode only a `/` line is an AppleWin line.
    if (mode == CommandMode::Monitor)
    {
        if (first == std::string::npos || line[first] != '/')
        {
            return ExecuteSessionLine (session, line, mode);
        }

        text = line.substr (first + 1);
    }

    stream.str (text);
    stream >> name;

    //  The argument is the rest of the line, so an expression with spaces is
    //  read whole.
    std::getline (stream, argument);
    argument.erase (0, std::min (argument.size(), argument.find_first_not_of (" \t")));
    argument.erase (argument.find_last_not_of (" \t") + 1);

    if (!name.empty() && !session.GetInterpreter().IsAssembling())
    {
        entry = AppleWinCommandTable::Find (name);
    }

    //  PANEL is the window's own: batch and the pipe report that it needs one.
    if (entry != nullptr && entry->verb == DebugVerb::ListPanels)
    {
        return ExecutePanelLine (session, text, line, mode);
    }

    if (entry == nullptr || entry->availability != CommandAvailability::WindowOnly)
    {
        return ExecuteSessionLine (session, line, mode);
    }

    reply.command = line;

    switch (entry->family)
    {
    case AppleWinCommandFamily::Cursor:
        m_codeViews.MoveCodePane (session, entry->name, reply);
        break;

    case AppleWinCommandFamily::MiniMemory:
        MoveMemoryPane (session, entry->name, argument, reply);
        break;

    case AppleWinCommandFamily::Window:
        ShowWindowPane (session, entry->name, reply);
        break;

    case AppleWinCommandFamily::Views:
        reply.SetError (CommandStatus::NotAvailable, "command not available",
                        std::format ("{} is not available here: the emulator window shows the screen.", entry->name));
        break;

    default:
        reply.SetError (CommandStatus::NotAvailable, "command not available",
                        std::format ("{} is not available here: the Casso theme sets the window's colors and font.", entry->name));
        break;
    }

    session.FormatReply (reply, mode);
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ExecuteSessionLine
//
//  A line the session runs. A script it starts runs each of its lines back
//  through this window, in the mode its command gives, so a script opens
//  panels and moves panes as the same lines typed would.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewCommands::ExecuteSessionLine (DebugSession & session, const std::string & line, CommandMode mode)
{
    DebugSession::ScriptLineRunner  previous = session.GetScriptLineRunner();
    Reply                           reply;



    session.SetScriptLineRunner ([this, &session] (const std::string & scriptLine, CommandMode scriptMode)
    {
        return ExecuteWindowLine (session, scriptLine, scriptMode);
    });

    reply = ExecuteLine (session, line, mode);
    session.SetScriptLineRunner (std::move (previous));
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ShowWindowPane
//
//  AppleWin's window commands switch its one screen between views. Here every
//  view is a pane, so they bring one forward: CODE and CODE1 the first
//  disassembly, CODE2 the second, DATA and DATA1 the first memory window,
//  DATA2 the second, CONSOLE the console. A second view that is not open
//  opens.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewCommands::ShowWindowPane (DebugSession & session, const std::string & name, Reply & reply)
{
    if (name.starts_with ("SOURCE"))
    {
        reply.SetError (CommandStatus::NotAvailable, "command not available", "SOURCE needs a link to an assembler listing.");
        return;
    }

    if (name == "CONSOLE")
    {
        m_showPane = DebuggerLayout::kConsole;
    }
    else if (name == "CODE" || name == "CODE1")
    {
        m_showPane = DebuggerLayout::GetCodePaneId (0);
    }
    else if (name == "CODE2")
    {
        if (!m_codeViews.IsCodeViewOpen (1))
        {
            m_codeViews.OpenCodeView (1, session.GetTarget().GetRegisters().pc);
        }

        m_showPane = DebuggerLayout::GetCodePaneId (1);
    }
    else if (name == "DATA" || name == "DATA1")
    {
        m_showPane = DebuggerLayout::GetMemoryPaneId (1);
    }
    else if (name == "DATA2")
    {
        if (!m_memoryWindows.GetMemoryWindowAddress (2).has_value())
        {
            GoToMemory (2, m_memoryWindows.GetMemoryAddress());
        }

        m_showPane = DebuggerLayout::GetMemoryPaneId (2);
    }
    else
    {
        reply.data = MessageData { { "This window shows every pane at once." } };
        return;
    }

    ++m_showPaneSerial;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::ExecutePanelLine
//
//  Parsed by the same parser batch uses, so the window and batch agree on
//  what a PANEL line means; only what it does differs.
//
////////////////////////////////////////////////////////////////////////////////

Reply DebuggerViewCommands::ExecutePanelLine (DebugSession & session, const std::string & text, const std::string & line, CommandMode mode)
{
    AppleWinParseResult  parsed = AppleWinParser::Parse (text, session);
    Reply                reply;



    reply.command = line;

    if (parsed.status != ParseStatus::Ok)
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", parsed.error);
    }
    else
    {
        m_panels.RunPanelCommand (session, parsed.command, reply);
    }

    session.FormatReply (reply, mode);
    return reply;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::MoveMemoryPane
//
//  Each memory window shows bytes and characters together, so MD, MA, MT and
//  M all move one: the 1 forms the first window, the 2 forms the second, as
//  DATA2 numbers it, opening it when it is not open.
//  The address is read as the memory address box reads it: an expression,
//  as every other AppleWin address is, alone or in a 6502 operand form.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewCommands::MoveMemoryPane (DebugSession & session, const std::string & name, const std::string & argument, Reply & reply)
{
    int          id      = name.ends_with ('2') ? 2 : 1;
    Word         address = 0;
    std::string  error;



    if (argument.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("{} needs an address.", name));
        return;
    }

    if (!MemoryGoTo::TryResolveMemoryAddress (session, argument, address, error))
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", error);
        return;
    }

    GoToMemory (id, address);
    reply.data = MessageData { { std::format ("Memory window {} is at ${:04X}.", id, address) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::GoToMemory
//
//  A memory pane places its view once and then follows its own scrolling, so
//  a new address alone would be scrolled straight back. It reaches the pane
//  as a Go to, as the Go to box's does.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewCommands::GoToMemory (int window, Word address)
{
    DebuggerViewSnapshot::GoTo  goTo;



    m_memoryWindows.OpenMemoryWindow (window, address);

    goTo.window  = window;
    goTo.text    = std::format ("{:04X}", address);
    goTo.address = address;
    goTo.serial  = m_goTo.has_value() ? m_goTo->serial + 1 : 1;

    m_goTo = goTo;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands::RequestGoTo
//
//  Resolved on the CPU thread, for the window to act on when the next
//  snapshot carries it.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerViewCommands::RequestGoTo (DebugSession & session, int window, const std::string & text)
{
    DebuggerViewSnapshot::GoTo  goTo;
    Word                        address = 0;



    goTo.window  = window;
    goTo.text    = text;
    goTo.serial  = m_goTo.has_value() ? m_goTo->serial + 1 : 1;

    if (MemoryGoTo::TryResolveMemoryAddress (session, text, address, goTo.error))
    {
        goTo.address = address;
    }

    m_goTo = goTo;
}
