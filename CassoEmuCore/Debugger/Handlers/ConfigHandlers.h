#pragma once

#include "Debugger/IDebugCommandHandler.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  ConfigHandlers
//
//  PWD, CD, RUN, LOAD, SAVE, STARTUP, DISASM, DISK, LOG, ECHO, PRINT,
//  PRINTF, CALC, ?, HELP, VERSION and MOTD, and PANEL outside the window,
//  which reports that it needs one.
//
//  RUN, LOAD and STARTUP execute a script's lines through the session and
//  collect every reply's text; a script whose lines failed fails too. SAVE
//  writes the breakpoint, watch, zero-page and bookmark scripts as one file of
//  AppleWin commands, which LOAD replays as AppleWin commands in any mode.
//
////////////////////////////////////////////////////////////////////////////////

class ConfigHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    //  How deep RUN, LOAD and STARTUP may nest, a script running a script.
    static constexpr int  kMaxScriptDepth = 16;

private:
    struct ScriptOutcome
    {
        int  failed          = 0;
        int  firstFailedLine = 0;
    };

    static constexpr const char * kStartupScript = "DebuggerAutoRun.txt";
    static constexpr int          kDiskSlot      = 6;
    static constexpr int          kHexDigits     = 4;
    static constexpr int          kBinaryDigits  = 8;

    static void  PrintDirectory  (DebugSession & session, Reply & reply);
    static void  ChangeDirectory (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         RunFile         (DebugSession & session, const std::string & name, std::optional<CommandMode> mode, Reply & reply);
    static void  RunScript       (DebugSession & session, const std::string & content, std::optional<CommandMode> mode,
                                  MessageData & output, ScriptOutcome & outcome);
    static void  SaveAll         (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         Disassembly     (const DebugCommand & command, Reply & reply);
    static void  Disk            (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Log             (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Echo            (const DebugCommand & command, Reply & reply);
    static void  Print           (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  PrintFormatted  (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Help            (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Output          (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  MessageOfTheDay (DebugSession & session, Reply & reply);

    static void  SplitItems      (const std::string & text, std::vector<std::string> & items);
    static bool  TryUnquote      (const std::string & item, std::string & text);
    static bool  TryEvaluate     (DebugSession & session, const std::string & text, Word & value, std::string & error);
    static char  GetPrintable    (Word value);
    static std::string  ToUpper  (const std::string & text);

    int  m_scriptDepth = 0;

    // DISASM settings. They are kept and reported; no disassembly reads them.
    std::map<std::string, bool>  m_disassembly =
    {
        { "BRANCH", true }, { "CLICK", false }, { "COLON", true }, { "FENCE", true },
        { "OPCODE", true }, { "POINTER", true }, { "SPACES", true }, { "TARGET", true },
    };
};
