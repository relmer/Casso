#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/IBlockSource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBlockSource
//
//  Blocks in memory: a WOZ image's decoded blocks, or a replay copy's blocks,
//  which every copy of the media shares. A write changes only the media's
//  overlay, and nothing is written to a file.
//
////////////////////////////////////////////////////////////////////////////////

class MemoryBlockSource : public IBlockSource
{
public:
    explicit MemoryBlockSource (std::shared_ptr<const std::vector<Byte>> blocks) : m_blocks (std::move (blocks)) {}

    HRESULT  ReadBlock         (uint32_t block, std::span<Byte> outBytes) override;
    HRESULT  WriteBlock        (uint32_t block, std::span<const Byte> bytes) override;
    HRESULT  RestoreWritten    (const WrittenBlocks & written) override;
    HRESULT  Flush             () override { return S_OK; }
    bool     HasPendingWrites  () const override { return false; }
    bool     HasChangedOnHost  () override { return false; }
    HRESULT  GetAttachedBlocks (std::shared_ptr<const std::vector<Byte>> & outBlocks) override;

private:
    std::shared_ptr<const std::vector<Byte>>  m_blocks;
};
