#pragma once

#include "Debugger/IDebugCommandHandler.h"
#include "Debugger/Events/IIoEventSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LogHandlers
//
//  VIDEOLOG and SOUNDLOG: the video mode changes, and the speaker toggles
//  and Mockingboard writes, each with the cycle it happened at. Entries are
//  added to both logs all the time, from the start of the session, through
//  the session's IIoEventSource; CLEAR hides the entries made so far, LIST
//  lists the newest, and SAVE writes every entry since the log was cleared,
//  rebuilt from history before where the event cache begins.
//
////////////////////////////////////////////////////////////////////////////////

class LogHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute  (DebugSession & session, const DebugCommand & command, Reply & reply) override;

private:
    static constexpr size_t        kDefaultListCount = 100;
    static constexpr size_t        kMaxCountDigits   = 9;
    static constexpr const char  * kDefaultVideoFile = "VideoLog.txt";
    static constexpr const char  * kDefaultSoundFile = "SoundLog.txt";

    static void  Execute     (DebugSession & session, const DebugCommand & command, IoLogKind kind, Reply & reply);
    static void  List        (IIoEventSource & source, IoLogKind kind, size_t count, std::vector<std::string> & lines, Reply & reply);
    static void  Save        (DebugSession & session, IIoEventSource & source, IoLogKind kind, const std::string & name, Reply & reply);
    static void  FormatTitle (const IoLogEntries & listing, std::vector<std::string> & lines);
};
