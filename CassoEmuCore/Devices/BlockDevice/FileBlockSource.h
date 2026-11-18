#pragma once

#include "Pch.h"

#include "Devices/BlockDevice/IBlockSource.h"

class BlockImageFile;





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockSource
//
//  A block image file. A write changes the file's blocks in memory, and Flush
//  writes them to the file. The source has each written block's content from
//  before its first write, so loading a state from before that write restores
//  the block, and a replay copy has the blocks as they were attached.
//
////////////////////////////////////////////////////////////////////////////////

class FileBlockSource : public IBlockSource
{
public:
    explicit FileBlockSource (std::unique_ptr<BlockImageFile> file);
    ~FileBlockSource() override;

    HRESULT  ReadBlock         (uint32_t block, std::span<Byte> outBytes) override;
    HRESULT  WriteBlock        (uint32_t block, std::span<const Byte> bytes) override;
    HRESULT  RestoreWritten    (const WrittenBlocks & written) override;

    //  Fails, writing nothing, when the file changed on the host since it was
    //  attached or last flushed.
    HRESULT  Flush             () override;
    bool     HasPendingWrites  () const override { return m_hasPendingWrites; }
    bool     HasChangedOnHost  () override;
    HRESULT  GetAttachedBlocks (std::shared_ptr<const std::vector<Byte>> & outBlocks) override;

private:
    std::unique_ptr<BlockImageFile>           m_file;
    std::map<uint32_t, std::vector<Byte>>     m_original;
    std::shared_ptr<const std::vector<Byte>>  m_attachedBlocks;
    bool                                      m_hasPendingWrites = false;
};
