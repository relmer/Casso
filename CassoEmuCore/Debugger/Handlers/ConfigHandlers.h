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
//  collect every reply's text. SAVE writes the breakpoint, watch, zero-page
//  and bookmark scripts as one file, which LOAD replays.
//
////////////////////////////////////////////////////////////////////////////////

class ConfigHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    // Each line executed in turn; a line beginning with ; is a comment.
    static void  RunScript (DebugSession & session, const std::string & content, MessageData & output);

private:
    static constexpr const char * kStartupScript = "DebuggerAutoRun.txt";
    static constexpr int          kDiskSlot      = 6;
    static constexpr int          kHexDigits     = 4;
    static constexpr int          kBinaryDigits  = 8;

    static void  PrintDirectory  (DebugSession & session, Reply & reply);
    static void  ChangeDirectory (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  RunFile         (DebugSession & session, const std::string & name, Reply & reply);
    static void  SaveAll         (DebugSession & session, const DebugCommand & command, Reply & reply);
    void         Disassembly     (const DebugCommand & command, Reply & reply);
    static void  Disk            (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Log             (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Echo            (const DebugCommand & command, Reply & reply);
    static void  Print           (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  PrintFormatted  (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Help            (const DebugCommand & command, Reply & reply);

    static void  SplitItems      (const std::string & text, std::vector<std::string> & items);
    static bool  TryUnquote      (const std::string & item, std::string & text);
    static bool  TryEvaluate     (DebugSession & session, const std::string & text, Word & value, std::string & error);
    static std::string  ToUpper  (const std::string & text);
    static const char * GetFamilyName (int family);

    // DISASM settings, kept for the disassembly views that read them.
    std::map<std::string, bool>  m_disassembly =
    {
        { "BRANCH", true }, { "CLICK", false }, { "COLON", true }, { "FENCE", true },
        { "OPCODE", true }, { "POINTER", true }, { "SPACES", true }, { "TARGET", true },
    };
};
