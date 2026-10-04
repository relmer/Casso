#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CallerLink
//
//  One call the code now running is still inside of by the stack-pointer
//  rule: since the call, the stack pointer has never been above where the
//  call left it. The call began at cycle, at callSite, with the stack
//  pointer at stackLevel. A JSR or BRK has its opcode at callSite; an
//  interrupt was taken in place of the instruction there.
//
////////////////////////////////////////////////////////////////////////////////

struct CallerLink
{
    uint64_t  cycle       = 0;
    Word      callSite    = 0;
    Byte      stackLevel  = 0;
    Byte      opcode      = 0;
    bool      isInterrupt = false;
};
