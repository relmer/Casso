#pragma once

#include "CommandLineOptions.h"

class IChannelClient;
class JsonValue;





////////////////////////////////////////////////////////////////////////////////
//
//  DebugAttachResult
//
////////////////////////////////////////////////////////////////////////////////

struct DebugAttachResult
{
    std::string  output;
    std::string  diagnostics;
    int          exitStatus = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebugAttachRunner
//
//  `CassoCli debug --attach <pid>`: a script run against a Casso that is
//  already running, through its debug channel.
//
//  THE SAME EXIT STATUSES AS A BATCH RUN, so a script cannot tell from its exit
//  code which way it was run. 1 is a command that failed; 3 is a run that did
//  not finish -- here either a budget stop or a run that outlasted --timeout,
//  which is paused before exiting so the machine is left where it stopped
//  rather than running on unwatched; 2 is a channel that closed under it.
//
//  EVERY RUN CARRIES A BUDGET. Nobody is watching an attached script, and a
//  run with no bound could leave it waiting for good.
//
////////////////////////////////////////////////////////////////////////////////

class DebugAttachRunner
{
public:
    static constexpr int  kOk            = 0;
    static constexpr int  kCommandFailed = 1;
    static constexpr int  kChannelClosed = 2;
    static constexpr int  kRunUnfinished = 3;

    //  How long to wait for a paused machine to report its stop.
    static constexpr DWORD  kPauseWaitMs = 5000;

    static void  Run (IChannelClient                         & client,
                      const CommandLineOptions::DebugOptions & options,
                      const std::string                      & scriptText,
                      DebugAttachResult                      & result);

    //  One command request, the line escaped as JSON.
    static std::string  BuildCommand (int64_t id, const std::string & line, const std::string & mode, uint64_t budget);

private:
    enum class Wait
    {
        Received,
        TimedOut,
        Closed,
    };

    static Wait  ReadUntil      (IChannelClient & client, DWORD timeoutMs, const std::function<bool (const JsonValue &, const std::string &)> & isWanted,
                                 const CommandLineOptions::DebugOptions & options, DebugAttachResult & result, JsonValue & found);
    static void  PrintRecord    (const JsonValue & record, const std::string & raw,
                                 const CommandLineOptions::DebugOptions & options, DebugAttachResult & result);
};
