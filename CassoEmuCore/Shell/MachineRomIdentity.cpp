#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Core/StateHash.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/RomDevice.h"
#include "Devices/RomGeneration.h"
#include "Machines/Apple2/Apple2c/Apple2cRomBank.h"
#include "Machines/Apple2/Common/CxxxRomRouter.h"
#include "Shell/MachineRomIdentity.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRomIdentity::Get
//
//  A hash over every ROM image the machine reads: the ROM devices in device
//  order, the //e internal $Cxxx ROM and the slot ROMs, then the //c
//  firmware banks. A debugger ROM patch changes it: a patched ROM is a
//  different ROM. On the //c the internal $Cxxx ROM is a slice of whichever
//  bank is switched in, so it is left out of the hash for the banks
//  themselves, and a state saved in one bank loads in the other.
//
//  Every snapshot carries it, so the hash is kept and taken again only once
//  RomGeneration says a ROM image changed since.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineRomIdentity::Get (const std::vector<std::unique_ptr<MemoryDevice>> & ownedDevices, const CxxxRomRouter * router, const Apple2cRomBank * romBank) const
{
    constexpr int          kRomBankCount = 2;
    uint64_t               hash          = StateHash::kSeed;
    const RomDevice      * rom           = nullptr;
    size_t                 size          = 0;
    int                    index         = 0;
    uint64_t               generation    = RomGeneration::Get();



    if (generation == m_generation)
    {
        return m_identity;
    }

    for (const std::unique_ptr<MemoryDevice> & owned : ownedDevices)
    {
        rom = dynamic_cast<const RomDevice *> (owned.get());

        if (rom != nullptr)
        {
            size = static_cast<size_t> (rom->GetEnd() - rom->GetStart()) + 1;
            hash = HashBytes (hash, rom->GetData(), size);
        }
    }

    for (index = 0; router != nullptr && index < DiskImageStore::kSlotCount; index++)
    {
        hash = HashBytes (hash, router->GetSlotRom (index));
    }

    if (router != nullptr && romBank == nullptr)
    {
        hash = HashBytes (hash, router->GetInternalRom());
    }

    for (index = 0; romBank != nullptr && index < kRomBankCount; index++)
    {
        hash = HashBytes (hash, romBank->GetBankImage (index));
    }

    m_identity           = hash;
    m_generation = generation;
    m_hashCount++;

    return hash;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRomIdentity::HashBytes
//
//  StateHash continuing from hash. Every snapshot carries the ROM identity,
//  so this runs at every keyframe; a byte-at-a-time hash over the
//  ROMs cost more than saving the rest of the machine.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineRomIdentity::HashBytes (uint64_t hash, const Byte * data, size_t size)
{
    return StateHash::Hash (data, size, hash);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRomIdentity::HashBytes
//
////////////////////////////////////////////////////////////////////////////////

uint64_t MachineRomIdentity::HashBytes (uint64_t hash, const std::vector<Byte> & bytes)
{
    return HashBytes (hash, bytes.data(), bytes.size());
}
