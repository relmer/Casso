#include "Pch.h"

#include "Debugger/Handlers/CallStackHandlers.h"

#include "Debugger/DebugSession.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHandlers::TryExecute
//
////////////////////////////////////////////////////////////////////////////////

bool CallStackHandlers::TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    switch (command.verb)
    {
    case DebugVerb::ShowCallStack:    reply.data = session.GetCallStack();    return true;
    case DebugVerb::SetCallStackMode: SetMode (session, command, reply);      return true;

    default:
        return false;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHandlers::SetMode
//
//  The parser has already checked the name; an empty one only reports.
//
////////////////////////////////////////////////////////////////////////////////

void CallStackHandlers::SetMode (DebugSession & session, const DebugCommand & command, Reply & reply)
{
    if (command.text == "RECORDED")
    {
        session.GetCallRecords().SetMechanism (CallStackMechanism::Recorded);
    }
    else if (command.text == "WALK")
    {
        session.GetCallRecords().SetMechanism (CallStackMechanism::Walk);
    }
    else if (command.text == "HYBRID")
    {
        session.GetCallRecords().SetMechanism (CallStackMechanism::Hybrid);
    }

    reply.data = CallStackModeData { session.GetCallRecords().GetMechanism() };
}
