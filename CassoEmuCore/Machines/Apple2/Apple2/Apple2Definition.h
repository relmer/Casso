#pragma once

#include "Pch.h"

#include "Machines/MachineDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2Definition
//
//  The Apple ][ of 1977: 48K on one bank, an uppercase-only keyboard, the
//  original soft switches, and the three video modes the machine shipped with.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2Definition
{
public:
    static const MachineDefinition &  Get();
};
