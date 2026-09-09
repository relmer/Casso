#pragma once

#include "Pch.h"

#include "Machines/MachineDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2PlusDefinition
//
//  The Apple ][ plus of 1979. Identical hardware to the ][ as far
//  as this definition is concerned: the differences that matter to
//  an owner are its ROM and what its slots carry, and both of those
//  remain the owner's to set.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2PlusDefinition
{
public:
    static const MachineDefinition &  Get();
};
