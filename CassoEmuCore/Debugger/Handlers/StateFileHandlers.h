#pragma once

#include "Debugger/IDebugCommandHandler.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  StateFileHandlers
//
//  SAVESTATE and LOADSTATE. The session's host saves or loads the machine
//  state file on the machine's own thread, as File > Save state and Load
//  state do, and reports the result there.
//
////////////////////////////////////////////////////////////////////////////////

class StateFileHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

private:
    static void         Request (DebugSession & session, const DebugCommand & command, Reply & reply);
    static std::string  Unquote (const std::string & text);
};
