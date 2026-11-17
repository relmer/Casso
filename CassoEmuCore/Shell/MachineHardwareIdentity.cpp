#include "Pch.h"

#include "Shell/MachineHardwareIdentity.h"

#include "Core/MachineConfig.h"
#include "Core/StateHash.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHardwareIdentity::Get
//
//  Each slot in the order the config lists it, then the machine's own ports.
//  A slot's ROM file isn't hashed here: the ROM identity has the ROMs' bytes.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHardwareIdentity::Get (const MachineConfig & config)
{
    uint64_t  hash = StateHash::kSeed;



    hash = HashValue (hash, config.slots.size());

    for (const SlotConfig & slot : config.slots)
    {
        hash = HashValue  (hash, (uint64_t) slot.slot);
        hash = HashString (hash, slot.device);
        hash = HashValue  (hash, slot.enabled ? 1 : 0);
        hash = HashPorts  (hash, slot.ports);
    }

    return HashPorts (hash, config.ports);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHardwareIdentity::HashPorts
//
//  A port's name, device and image, and the chain behind it.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHardwareIdentity::HashPorts (uint64_t hash, const std::vector<PortConfig> & ports)
{
    hash = HashValue (hash, ports.size());

    for (const PortConfig & port : ports)
    {
        hash = HashString (hash, port.name);
        hash = HashString (hash, port.device);
        hash = HashString (hash, port.image);
        hash = HashPorts  (hash, port.chain);
    }

    return hash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHardwareIdentity::HashString
//
//  The length first, so "ab" then "c" differs from "a" then "bc".
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHardwareIdentity::HashString (uint64_t hash, const std::string & text)
{
    hash = HashValue (hash, text.size());

    return StateHash::Hash (reinterpret_cast<const Byte *> (text.data()), text.size(), hash);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineHardwareIdentity::HashValue
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineHardwareIdentity::HashValue (uint64_t hash, uint64_t value)
{
    return StateHash::Hash (reinterpret_cast<const Byte *> (&value), sizeof (value), hash);
}
