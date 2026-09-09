#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinition
//
//  What a machine IS, as opposed to how its owner set it up.
//
//  Nobody chose the //e's keyboard or the ][+'s soft switches; those are the
//  machine. They live here, in code, because a value nobody may change has no
//  business in a file anyone may edit. The preferences store used to merge
//  user deltas into the device list, which meant a delta could give a //c an
//  original keyboard or a ][ the //e soft switches and nothing refused it.
//
//  What an owner COULD change on the physical machine stays in the JSON
//  definition: what is in each slot, what is plugged into each port, and which
//  ROM file to load. The test is whether it could have been done with a
//  screwdriver.
//
//  Devices are still built through ComponentRegistry by type string. What
//  moved is only the decision of WHICH devices a model has.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineDefinition
{
    //  The model's directory name under Resources/Machines, e.g. "Apple2e".
    std::string                  id;

    std::string                  cpu;
    std::string                  cpuManufacturer;

    std::vector<RamRegion>       ram;
    std::vector<InternalDevice>  internalDevices;
    std::vector<std::string>     videoModes;
    std::string                  keyboardType;
};
