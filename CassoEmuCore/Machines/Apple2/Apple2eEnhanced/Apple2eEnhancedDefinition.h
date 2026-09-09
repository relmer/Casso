#pragma once

#include "Pch.h"

#include "Machines/MachineDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2eEnhancedDefinition
//
//  The Enhanced //e of 1985. Its device list is byte-for-byte the
//  //e's; the enhancement is the 65C02 and the ROMs that use it.
//  Stating the list again rather than deriving it keeps the answer
//  to "what is this machine" in one file per machine.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2eEnhancedDefinition
{
public:
    static const MachineDefinition &  Get();
};
