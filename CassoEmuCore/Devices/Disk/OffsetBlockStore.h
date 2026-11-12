#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  OffsetBlockStore
//
//  A run of another store's blocks as a separate store, such as one
//  partition of a card. Block 0 is the run's first. The other store must
//  outlive this one, and a commit commits it.
//
////////////////////////////////////////////////////////////////////////////////

class OffsetBlockStore : public IBlockStore
{
public:
    OffsetBlockStore (IBlockStore & store, uint32_t first, uint32_t count) : m_store (store), m_first (first), m_count (count) {}

    uint32_t  GetBlockCount () const override { return m_count; }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override;
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override;
    HRESULT   Commit        () override { return m_store.Commit(); }

private:
    IBlockStore  & m_store;
    uint32_t       m_first = 0;
    uint32_t       m_count = 0;
};
