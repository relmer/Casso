#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineDefinition
//
//  One machine's answers, flattened.
//
//  The machines themselves are a class hierarchy rooted at the Apple ][ (see
//  IMachine). This is what a lookup hands back: the same facts as plain data,
//  so the config loader and the settings pages read fields instead of calling
//  virtuals they would gain nothing from.
//
//  Everything here is invariant. Which CPU a //e Enhanced has is not a setting,
//  and none of these values comes from a file.
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

    //  Zero on a //c, which has ports instead. A count, not a facet removed.
    int                          slotCount   = 0;

    //  False from the //e onward, where the soft-switch bank owns the paddle
    //  timer and PREAD.
    bool                         hasGamePort = false;
};
