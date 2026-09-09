#pragma once

#include "Pch.h"

#include "Machines/MachineDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2eDefinition
//
//  The Apple //e of 1983, and the machine that introduced almost
//  everything the later models inherit: an auxiliary RAM bank and
//  the MMU that pages it, extended soft switches, a full keyboard
//  with lowercase and modifiers, and 80-column and double hi-res
//  video.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2eDefinition
{
public:
    static const MachineDefinition &  Get();
};
