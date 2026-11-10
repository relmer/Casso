#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CachingBlockStore
//
//  A slow block store, such as a card read through a pipe, with the blocks
//  read from it cached and the blocks written to it in memory until `Commit`.
//
//  A written block equal to the block already there is dropped, so writing
//  a whole partition back sends only the blocks that differ. `Commit` sends
//  them in runs, and `Discard` drops them for a write the user declined.
//
//  The cache is never refreshed: a store changed from elsewhere is read as it
//  was. A new CachingBlockStore is made each time a card is opened.
//
////////////////////////////////////////////////////////////////////////////////

class CachingBlockStore : public IBlockStore
{
public:
    explicit CachingBlockStore (IBlockStore & inner) : m_inner (inner) {}

    uint32_t  GetBlockCount () const override { return m_inner.GetBlockCount(); }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override;
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override;
    HRESULT   Commit        () override;

    size_t    GetChangedBlockCount () const { return m_changed.size(); }
    void      Discard              ()       { m_changed.clear(); }

private:
    static constexpr uint32_t  kChunkBlocks = 128;

    HRESULT  LoadChunk (uint32_t chunk, const std::vector<Byte> * & outBytes);

    IBlockStore                            & m_inner;

    //  By chunk number. The last chunk of the store may be short.
    std::map<uint32_t, std::vector<Byte>>    m_chunks;

    //  By block number.
    std::map<uint32_t, std::vector<Byte>>    m_changed;
};