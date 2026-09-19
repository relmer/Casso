#pragma once

#include "Debugger/IDebugCommandHandler.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  CallStackHandlers
//
//  CALLS: the chain of calls to PC, innermost first, by the session's
//  mechanism (FR-067 to FR-069). CALLS MODE reports the mechanism, and CALLS
//  MODE RECORDED|WALK|HYBRID chooses it for the session, which the pane
//  shares.
//
////////////////////////////////////////////////////////////////////////////////

class CallStackHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

private:
    static void  SetMode (DebugSession & session, const DebugCommand & command, Reply & reply);
};
