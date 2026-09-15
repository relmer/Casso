#pragma once

#include "Debugger/Reply.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  IDebugCommandHandler
//
//  One family of commands. The session offers each command to its registered
//  families in turn; a family that does not handle the verb returns false and
//  leaves the reply untouched.
//
////////////////////////////////////////////////////////////////////////////////

class IDebugCommandHandler
{
public:
    virtual ~IDebugCommandHandler() = default;

    virtual bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) = 0;
};
