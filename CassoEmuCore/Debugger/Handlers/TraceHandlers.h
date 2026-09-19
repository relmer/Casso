#pragma once

#include "Debugger/IDebugCommandHandler.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  TraceHandlers
//
//  HISTORY: ON and OFF switch the target's instruction trace, SAVE writes
//  every retained entry to a file, and HISTORY [first [count]] reports a
//  window of entries, the newest when no first is given. Each entry is
//  reported with its instruction disassembled and the symbols for its
//  address and the address it accessed.
//
////////////////////////////////////////////////////////////////////////////////

class TraceHandlers : public IDebugCommandHandler
{
public:
    static constexpr uint32_t  kDefaultCount = 20;

    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    //  Fills each entry's instruction text and symbols.
    static void  Describe (DebugSession & session, std::vector<TraceRecord> & entries);

private:
    static void  Show   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Switch (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  Save   (DebugSession & session, const DebugCommand & command, Reply & reply);
};
