#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IBlockStore
//
//  A run of 512-byte blocks read and written in place, for volumes and cards
//  too large to copy whole on every write. Writes take effect on `Commit`.
//
////////////////////////////////////////////////////////////////////////////////

class IBlockStore
{
public:
    static constexpr size_t  kBlockBytes = 512;

    virtual ~IBlockStore() = default;

    virtual uint32_t  GetBlockCount () const = 0;
    virtual HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) = 0;
    virtual HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) = 0;
    virtual HRESULT   Commit        () = 0;
};
