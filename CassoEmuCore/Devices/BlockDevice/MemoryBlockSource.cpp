#include "Pch.h"

#include "Devices/BlockDevice/MemoryBlockSource.h"

#include "Devices/BlockDevice/BlockDeviceMedia.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBlockSource::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBlockSource::ReadBlock (uint32_t block, std::span<Byte> outBytes)
{
    constexpr size_t  kBlockBytes = BlockDeviceMedia::kBlockBytes;



    std::copy_n (m_blocks->begin() + (ptrdiff_t) ((size_t) block * kBlockBytes), kBlockBytes, outBytes.begin());

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBlockSource::WriteBlock
//
//  The blocks are shared, so a write changes only the media's overlay.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBlockSource::WriteBlock (uint32_t block, std::span<const Byte> bytes)
{
    (void) block;
    (void) bytes;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBlockSource::RestoreWritten
//
//  The overlay is the only place a written block is, so a load has nothing
//  to put back here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBlockSource::RestoreWritten (const WrittenBlocks & written)
{
    (void) written;

    return S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBlockSource::GetAttachedBlocks
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MemoryBlockSource::GetAttachedBlocks (std::shared_ptr<const std::vector<Byte>> & outBlocks)
{
    outBlocks = m_blocks;

    return S_OK;
}
