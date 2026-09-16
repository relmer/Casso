#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  DebugCommandPayload
//
//  One debugger command line on its way to the CPU thread, with the id of the
//  client whose reply it is.
//
//  The CPU command queue carries a string, so the two travel as one:
//  "<clientId>\n<line>". A newline separates them because a command line never
//  contains one -- the channel splits requests on newlines before any command
//  reaches here -- so the line needs no escaping and arrives exactly as typed.
//
////////////////////////////////////////////////////////////////////////////////

struct DebugCommandPayload
{
    uint32_t     clientId = 0;
    std::string  line;

    static std::string  Encode    (uint32_t clientId, const std::string & line);
    static bool         TryDecode (const std::string & payload, DebugCommandPayload & decoded);
};
