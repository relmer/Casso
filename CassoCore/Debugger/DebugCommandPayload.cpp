#include "Pch.h"

#include "Debugger/CommandModeNames.h"
#include "Debugger/DebugCommandPayload.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebugCommandPayload::Encode
//
////////////////////////////////////////////////////////////////////////////////

std::string DebugCommandPayload::Encode (uint32_t clientId, const std::string & line, std::optional<CommandMode> mode)
{
    std::string  header = std::to_string (clientId);



    if (mode.has_value())
    {
        header += " ";
        header += CommandModeNames::GetName (*mode);
    }

    return header + "\n" + line;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebugCommandPayload::TryDecode
//
//  A payload with no separator, a client id that is not a whole number, or a
//  mode that is not one of the mode names, is rejected rather than guessed
//  at. The reply has to go somewhere, and sending it to a client chosen by a
//  malformed number would answer the wrong one.
//
//  An empty line is accepted. It is a command the session answers like any
//  other, and dropping it here would turn a blank request into silence.
//
////////////////////////////////////////////////////////////////////////////////

bool DebugCommandPayload::TryDecode (const std::string & payload, DebugCommandPayload & decoded)
{
    size_t                      separator = payload.find ('\n');
    size_t                      space     = payload.find (' ');
    size_t                      idEnd     = separator;
    uint64_t                    id        = 0;
    CommandMode                 parsed    = CommandMode::AppleWin;
    std::optional<CommandMode>  mode;



    if (separator == std::string::npos || separator == 0)
    {
        return false;
    }

    if (space < separator)
    {
        idEnd = space;

        if (!CommandModeNames::TryParse (payload.substr (space + 1, separator - space - 1), parsed))
        {
            return false;
        }

        mode = parsed;
    }

    if (idEnd == 0)
    {
        return false;
    }

    for (size_t i = 0; i < idEnd; i++)
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
    decoded.mode     = mode;

    return true;
}
