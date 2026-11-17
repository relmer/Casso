#pragma once

#include "Pch.h"

struct MachineConfig;
struct PortConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHardwareIdentity
//
//  A hash of the hardware a machine is built with: each slot's card and
//  whether it is enabled, and each port's device and image, the //c's
//  machine ports and a disk port's chain included. A saved state carries it,
//  and a state saved on other hardware doesn't load, since its parts would
//  be laid onto devices that are not the ones it was taken from.
//
////////////////////////////////////////////////////////////////////////////////

class MachineHardwareIdentity
{
public:
    static uint64_t  Get (const MachineConfig & config);

private:
    static uint64_t  HashPorts  (uint64_t hash, const std::vector<PortConfig> & ports);
    static uint64_t  HashString (uint64_t hash, const std::string & text);
    static uint64_t  HashValue  (uint64_t hash, uint64_t value);
};
