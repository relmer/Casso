#pragma once

#include "Pch.h"

#include "Machines/MachineDefinition.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinitions
//
//  Every machine Casso can be, looked up by the id its resource directory
//  uses. A model whose id is not here cannot be built, which is the point:
//  the set of machines that exist is a fact about this program, not about
//  what happens to be on disk.
//
////////////////////////////////////////////////////////////////////////////////

class MachineDefinitions
{
public:
    //  Returns nullptr when no shipped machine carries that id.
    static const MachineDefinition *  Find (const std::string & machineId);

    static std::vector<std::string>   GetKnownIds ();
};
