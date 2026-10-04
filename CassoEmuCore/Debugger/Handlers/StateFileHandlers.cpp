#include "Pch.h"

#include "Debugger/Handlers/StateFileHandlers.h"

#include "Core/TextEncoding.h"
#include "Debugger/DebugSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  StateFileHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool StateFileHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::SaveState:
    case DebugVerb::LoadState:
        Request (session, command, reply);
        return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StateFileHandlers::Request
//
//  The file name is resolved against the session's directory, as BSAVE's is,
//  and handed to the host. The save or load itself happens between
//  instructions on the machine's thread, so the reply says it was started.
//
////////////////////////////////////////////////////////////////////////////////

void StateFileHandlers::Request (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    const DebugSession::StateFileRequester  & requester = session.GetStateFileRequester();
    bool                                      isLoad    = command.verb == DebugVerb::LoadState;
    std::string                               name      = Unquote (command.text);
    std::wstring                              path;
    bool                                      isTaken   = false;



    if (name.empty())
    {
        reply.SetError (CommandStatus::Error, "invalid arguments", std::format ("{} needs a file name.", command.sourceName));
        return;
    }

    path = session.ResolvePath (name);

    if (requester != nullptr)
    {
        isTaken = requester (isLoad ? StateFileRequest::Load : StateFileRequest::Save, path);
    }

    if (!isTaken)
    {
        reply.SetError (CommandStatus::NotAvailable, "command not available",
                        std::format ("{} needs the emulator's machine.", command.sourceName));
        return;
    }

    reply.data = MessageData { { std::format ("{} the machine state {} {}.",
                                              isLoad ? "Loading" : "Saving",
                                              isLoad ? "from" : "to",
                                              TextEncoding::WideToNarrow (path)) } };
}





////////////////////////////////////////////////////////////////////////////////
//
//  StateFileHandlers::Unquote
//
////////////////////////////////////////////////////////////////////////////////

std::string StateFileHandlers::Unquote (const std::string & text)
{
    bool  isQuoted = text.size() >= 2
                  && (text.front() == '"' || text.front() == '\'')
                  && text.back() == text.front();



    return isQuoted ? text.substr (1, text.size() - 2) : text;
}
