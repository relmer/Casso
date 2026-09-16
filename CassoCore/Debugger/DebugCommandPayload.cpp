#include "Pch.h"

#include "Debugger/DebugCommandPayload.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugCommandPayload::Encode
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugCommandPayload::Encode (uint32_t clientId, const std::string & line)
{
    return std::to_string (clientId) + "\n" + line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugCommandPayload::TryDecode
//
//  A payload with no separator, or a client id that is not a whole number, is
//  refused rather than guessed at. The reply has to go somewhere, and sending
//  it to a client chosen by a malformed number would answer the wrong one.
//
//  An empty line is accepted. It is a command the session answers like any
//  other, and refusing it here would turn a blank request into silence.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugCommandPayload::TryDecode (const std::string & payload, DebugCommandPayload & decoded)
{
    size_t    separator = payload.find ('\n');
    uint64_t  id        = 0;



    if (separator == std::string::npos || separator == 0)
    {
        return false;
    }

    for (size_t i = 0; i < separator; i++)
    {
        char  digit = payload[i];

        if (digit < '0' || digit > '9')
        {
            return false;
        }

        id = id * 10 + (uint64_t) (digit - '0');

        if (id > UINT32_MAX)
        {
            return false;
        }
    }

    decoded.clientId = (uint32_t) id;
    decoded.line     = payload.substr (separator + 1);

    return true;
}
