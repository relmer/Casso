#pragma once

#include "Debugger/Reply.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewSnapshot
//
//  Everything the debugger window shows, as plain data.
//
//  A COPY, NOT A VIEW OF THE SESSION. The session belongs to the CPU thread and
//  the window to the UI thread, so the window never reads the session: the CPU
//  thread builds one of these and hands it across, and the window paints
//  whatever it last received.
//
////////////////////////////////////////////////////////////////////////////////

struct DebuggerViewSnapshot
{
    struct CodeLine
    {
        Word         address       = 0;
        std::string  bytes;
        std::string  instruction;
        std::string  symbol;
        bool         isCurrent     = false;
        bool         hasBreakpoint = false;
    };

    struct RegisterRow
    {
        std::string  name;
        std::string  value;
    };

    struct MemoryLine
    {
        Word         address = 0;
        std::string  bytes;
        std::string  characters;
        std::string  region;
    };

    struct StackLine
    {
        Word  address = 0;
        Byte  value   = 0;
    };

    struct BreakpointLine
    {
        int          id      = 0;
        Word         address = 0;
        std::string  text;
        bool         enabled = true;
    };

    struct WatchLine
    {
        int          id      = 0;
        Word         address = 0;
        std::string  value;
    };

    Word                         pc     = 0;
    CommandMode                  mode   = CommandMode::AppleWin;
    std::vector<CodeLine>        code;
    std::vector<RegisterRow>     registers;
    std::string                  flags;
    std::vector<MemoryLine>      memory;
    std::vector<StackLine>       stack;
    std::vector<BreakpointLine>  breakpoints;
    std::vector<WatchLine>       watches;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerViewState
//
//  What the window shows and what its controls do, with no window in it.
//
//  EVERY PANE IS A COMMAND'S REPLY. The code pane is `U`, the registers `R`,
//  memory `D`, the stack `STACK`, watches `WL`, breakpoints `BPL` -- run through
//  the session and read back from their typed data. The window therefore
//  cannot show anything batch mode or a channel client would not be told, and
//  a fix to a command is a fix to its pane.
//
//  EVERY CONTROL IS A COMMAND LINE. Clicking a line, stepping, poking a byte:
//  each produces the line a person could have typed, which the window then runs
//  exactly as it runs the command line. There is one path from an action to the
//  machine, and it is the one the rest of the debugger already tests.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerViewState
{
public:
    static constexpr int  kCodeLines   = 20;
    static constexpr int  kMemoryRows  = 16;

    //  Where the code and memory panes start. The code pane follows the PC
    //  unless the user moved it; the memory pane starts at the zero page.
    void  SetCodeAddress   (std::optional<Word> address) { m_codeAddress = address; }
    void  SetMemoryAddress (Word address)                { m_memoryAddress = address; }

    std::optional<Word>  GetCodeAddress   () const { return m_codeAddress; }
    Word                 GetMemoryAddress () const { return m_memoryAddress; }

    //  Runs the pane commands against the session. CPU thread only.
    DebuggerViewSnapshot  Build (DebugSession & session) const;

    //  The command a control stands for.
    static std::string  GetToggleBreakpointLine (const DebuggerViewSnapshot & snapshot, Word address);
    static std::string  GetPokeLine             (Word address, Byte value);
    static std::string  GetStepLine             ()             { return "T"; }
    static std::string  GetStepOverLine         ()             { return "P"; }
    static std::string  GetRunLine              ()             { return "G"; }
    static std::string  GetRunToCursorLine      (Word address);

    //  A line from the window's command box, run and formatted exactly as batch
    //  mode runs and formats it. CPU thread only.
    static Reply  ExecuteLine (DebugSession & session, const std::string & line, CommandMode mode);

    //  The R or W a line holds with no file name, which the window asks for,
    //  and the line with the chosen name added.
    static std::optional<DebugVerb>  GetMissingFileVerb  (const std::string & line, CommandMode mode);
    static std::string               GetLineWithFileName (const std::string & line, const std::string & path);

    static std::string  GetRegionLabel (MemoryRegion region);

private:
    std::optional<Word>  m_codeAddress;
    Word                 m_memoryAddress = 0x0000;
};
