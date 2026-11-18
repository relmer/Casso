#pragma once

#include "Pch.h"

class Apple2cRomBank;
class CxxxRomRouter;
class MemoryDevice;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineRomIdentity
//
//  A hash of every ROM image a machine was built with, which a saved state
//  has so that it loads only on the same ROM set. The hash is worked out
//  again only after a ROM image is patched or replaced, and GetHashCount
//  returns how many times it was worked out.
//
////////////////////////////////////////////////////////////////////////////////

class MachineRomIdentity
{
public:
    uint64_t  Get          (const std::vector<std::unique_ptr<MemoryDevice>> & ownedDevices, const CxxxRomRouter * router, const Apple2cRomBank * romBank) const;
    uint64_t  GetHashCount () const noexcept { return m_hashCount; }

private:
    static uint64_t  HashBytes (uint64_t hash, const Byte * data, size_t size);
    static uint64_t  HashBytes (uint64_t hash, const std::vector<Byte> & bytes);

    mutable uint64_t  m_identity   = 0;
    mutable uint64_t  m_generation = 0;
    mutable uint64_t  m_hashCount  = 0;
};
