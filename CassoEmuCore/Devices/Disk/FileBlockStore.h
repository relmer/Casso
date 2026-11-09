#pragma once

#include "Pch.h"

#include "Devices/Disk/IBlockStore.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockStore
//
//  Blocks read from a file a page at a time, as they are asked for, and kept
//  until the store is destroyed. A write changes the page in memory, and
//  `Commit` writes every changed page back in place. The blocks start at an
//  offset in the file, so a store can cover a container's payload or one
//  partition of a larger image.
//
////////////////////////////////////////////////////////////////////////////////

class FileBlockStore : public IBlockStore
{
public:
    static constexpr uint32_t  kBlocksPerPage = 128;

    FileBlockStore (IDiskFileIo & fileIo, std::string path, uint64_t firstByte, uint32_t blockCount);

    uint32_t  GetBlockCount () const override { return m_blockCount; }
    HRESULT   ReadBlocks    (uint32_t first, uint32_t count, std::span<Byte> outBytes) override;
    HRESULT   WriteBlocks   (uint32_t first, std::span<const Byte> bytes) override;
    HRESULT   Commit        () override;

    size_t    GetDirtyPageCount() const { return m_dirty.size(); }

private:
    HRESULT   LoadPage (uint32_t page, std::vector<Byte> *& outPage);

    IDiskFileIo                                 & m_fileIo;
    std::string                                   m_path;
    uint64_t                                      m_firstByte  = 0;
    uint32_t                                      m_blockCount = 0;
    std::map<uint32_t, std::vector<Byte>>         m_pages;
    std::set<uint32_t>                            m_dirty;
};
