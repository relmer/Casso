#pragma once

#include "Ui/Debugger/DebuggerActions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SessionActions
//
//  The actions on the session and its views: the command mode, the trace,
//  the call stack's mechanism, history saves, device panels, the profiler,
//  the video and sound logs, debug projects and the BASIC program.
//
////////////////////////////////////////////////////////////////////////////////

class SessionActions
{
public:
    static DebuggerAction  GetPanel         (const std::string & id, bool open, CommandMode mode);
    static DebuggerAction  GetTraceToggle   (bool isOn, CommandMode mode);
    static DebuggerAction  GetSetMode       (CommandMode target, CommandMode mode);
    static DebuggerAction  GetCallStackMode (const std::string & mechanism, CommandMode mode);
    static DebuggerAction  GetSaveHistory   (const std::string & path, CommandMode mode);
    static DebuggerAction  GetProfile       (const std::string & words, CommandMode mode);

    //  VIDEOLOG or SOUNDLOG with its words: CLEAR, or SAVE and a path.
    static DebuggerAction  GetIoLog      (bool isVideo, const std::string & words, CommandMode mode);
    static DebuggerAction  GetLoadBasic  (const std::string & path, CommandMode mode);
    static DebuggerAction  GetSaveBasic  (const std::string & path, CommandMode mode);
    static DebuggerAction  GetClearBasic (CommandMode mode);
    static DebuggerAction  GetProject    (const std::string & subcommand, const std::string & path, CommandMode mode);
};
