#pragma once

#include "Debugger/DebugCommand.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugCommandPayload
//
//  One debugger command line on its way to the CPU thread, with the id of the
//  client whose reply it is, and the mode to read it in when that is not the
//  session's own.
//
//  The CPU command queue carries a string, so they travel as one:
//  "<clientId>\n<line>", or "<clientId> <mode>\n<line>" with a mode. A newline
//  separates them because a command line never contains one -- the channel
//  splits requests on newlines before any command reaches here -- so the line
//  needs no escaping and arrives exactly as typed.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugCommandPayload
{
    uint32_t                    clientId = 0;
    std::string                 line;
    std::optional<CommandMode>  mode;

    static std::string  Encode    (uint32_t clientId, const std::string & line, std::optional<CommandMode> mode = std::nullopt);
    static bool         TryDecode (const std::string & payload, DebugCommandPayload & decoded);
};
