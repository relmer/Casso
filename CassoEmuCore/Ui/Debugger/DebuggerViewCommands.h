#pragma once

#include "Ui/Debugger/BreakpointHistory.h"
#include "Ui/Debugger/BreakpointImport.h"
#include "Ui/Debugger/DebuggerActions.h"
#include "Ui/Debugger/DebuggerViewSnapshot.h"

class CodeViews;
class DebugSession;
class DevicePanelViews;
class MemoryWindowViews;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewCommands
//
//  The window's command lines and control actions, run on the CPU thread:
//  console lines, control actions, the Breakpoints pane's undo steps, and the
//  AppleWin pane commands that move a pane or bring it forward. The pane to
//  bring forward, the reply to box and the Go to are in the next snapshot.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerViewCommands
{
public:
    DebuggerViewCommands (CodeViews & codeViews, MemoryWindowViews & memoryWindows, DevicePanelViews & panels);

    //  A line from the window's command box, run and formatted exactly as batch
    //  mode runs and formats it. CPU thread only.
    static Reply  ExecuteLine (DebugSession & session, const std::string & line, CommandMode mode);

    //  The same, except that the AppleWin names batch and the pipe report as
    //  needing the window are carried out on the panes here. CPU thread only.
    Reply  ExecuteWindowLine (DebugSession & session, const std::string & line, CommandMode mode);

    //  The lines the window's console shows for a line: the line behind the
    //  prompt of the mode it runs in, then the reply. It runs in `lineMode`
    //  when given, otherwise in the session's mode. CPU thread only.
    std::vector<std::string>  ExecuteConsoleLine (DebugSession & session, const std::string & line, std::optional<CommandMode> lineMode = std::nullopt);
    std::vector<std::string>  RunPendingScript   (DebugSession & session);

    //  The lines the console shows for a control's action: its echo behind
    //  the prompt, then the reply. The command runs directly, never through a
    //  parser (FR-135). CPU thread only.
    std::vector<std::string>  ExecuteAction (DebugSession & session, const DebuggerAction & action);

    //  A gap ahead of a command's lines, which sets the command and its output
    //  apart from the one before. No lines get none. The gap is a line of its
    //  own, kCommandGap, which the console draws kCommandGapHeight of a line
    //  high.
    static void  AddCommandGap (std::vector<std::string> & lines);
    static bool  IsCommandGap  (const std::string & line) { return line == kCommandGap; }

    static constexpr const char * kCommandGap       = "\f";
    static constexpr float        kCommandGapHeight = 0.75f;

    //  A breakpoints pane action as one undo step, or its undo or redo:
    //  each action's echo behind the prompt, then that action's reply.
    //  An import ends with how many of the file's lines were skipped.
    //  CPU thread only.
    std::vector<std::string>  ExecuteBreakpointStep (DebugSession & session, const BreakpointStep & step, bool isEchoed = true);

    //  The Breakpoints pane's undo history as a debug project saves it, and
    //  that history replaced from such text. CPU thread only.
    std::string  GetBreakpointUndoText () const                                         { return m_breakpointHistory.ToText(); }
    void         SetBreakpointUndoText (DebugSession & session, const std::string & text) { m_breakpointHistory.FromText (session, text); }

    //  Resolves a Go to on the CPU thread, for the window to act on when the
    //  next snapshot has it.
    void  RequestGoTo (DebugSession & session, int window, const std::string & text);

    //  Copies the Go to, the pane to bring forward, the last action's reply,
    //  the suggestion and the breakpoints' undo state into the snapshot.
    void  AddRequests (DebuggerViewSnapshot & snapshot) const;

private:
    void   MoveMemoryPane       (DebugSession & session, const std::string & name, const std::string & argument, Reply & reply);
    void   GoToMemory           (int window, Word address);
    void   ShowWindowPane       (DebugSession & session, const std::string & name, Reply & reply);
    void   ImportBreakpoints    (DebugSession & session, const std::string & path, const BreakpointImport::LineRunner & run, std::vector<std::string> & lines);
    Reply  ExecutePanelLine     (DebugSession & session, const std::string & text, const std::string & line, CommandMode mode);
    Reply  ExecuteSessionLine   (DebugSession & session, const std::string & line, CommandMode mode);
    Reply  ExecuteActionCommand (DebugSession & session, const DebuggerAction & action);
    void   AddActionLines       (DebugSession & session, const DebuggerAction & action, const Reply & reply, std::vector<std::string> & lines);

    //  The action's command with its definition parsed and what the user
    //  typed evaluated, or false with why not.
    static bool  TryResolveCommand (DebugSession & session, const DebuggerAction & action, DebugCommand & command, std::string & error);

    CodeViews                                & m_codeViews;
    MemoryWindowViews                        & m_memoryWindows;
    DevicePanelViews                         & m_panels;

    std::optional<DebuggerViewSnapshot::GoTo>    m_goTo;
    std::wstring                                 m_showPane;
    uint32_t                                     m_showPaneSerial     = 0;
    std::string                                  m_suggestion;
    uint32_t                                     m_suggestionSerial   = 0;
    std::vector<std::string>                     m_actionReply;
    uint32_t                                     m_actionReplySerial  = 0;
    std::wstring                                 m_actionReplyPane;
    bool                                         m_isActionReplyBoxed = false;
    bool                                         m_isActionReplyError = false;
    BreakpointHistory                            m_breakpointHistory;
};
