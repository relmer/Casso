#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBlockStore
//
//  Blocks in a buffer, for the tests and for a volume small enough to load
//  whole.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryBlockStore : public IBlockStore
{
public:
    explicit MemoryBlockStore (std::vector<Byte> bytes) : m_bytes (std::move (bytes)) {}

    uint32_t  GetBlockCount () const override { return (uint32_t) (m_bytes.size() / kBlockBytes); }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override;
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override;
    HRESULT   Commit        () override { return S_OK; }

    const std::vector<Byte> &  GetBytes() const { return m_bytes; }

private:
    std::vector<Byte>  m_bytes;
};
