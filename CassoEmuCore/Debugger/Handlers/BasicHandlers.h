#pragma once

#include "Debugger/IDebugCommandHandler.h"

class DebugSession;





////////////////////////////////////////////////////////////////////////////////
//
//  BasicHandlers
//
//  Applesoft: BPBASIC, BPBASICVAR and BRKBASIC set its breakpoints, which
//  the breakpoint commands then manage as any other; BASICVARS and
//  BASICSTACK list the program's variables and its GOSUB and FOR frames.
//  The steps, BASICSTEP, BASICLINE and BASICOUT, are runs, which the session
//  starts itself.
//
////////////////////////////////////////////////////////////////////////////////

class BasicHandlers : public IDebugCommandHandler
{
public:
    bool  TryExecute (DebugSession & session, const DebugCommand & command, Reply & reply) override;

    //  BPBASIC's line number, 0 to 63999 in decimal as Applesoft takes it.
    static bool  TryParseLine (const std::string & text, Word & line);

private:
    static void  SetLine       (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetVariable   (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  SetErrorBreak (DebugSession & session, const DebugCommand & command, Reply & reply);
    static void  ShowVariables (DebugSession & session, Reply & reply);
    static void  ShowStack     (DebugSession & session, Reply & reply);
    static bool  HasErrorBreak (DebugSession & session);

    static constexpr Word  kMaxLine = 63999;
};
