#include "Pch.h"

#include "Devices/BlockDevice/FileBlockSource.h"

#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/Disk/IBlockStore.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockSource
//
////////////////////////////////////////////////////////////////////////////////

FileBlockSource::FileBlockSource (std::unique_ptr<BlockImageFile> file) :
    m_file (std::move (file))
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ~FileBlockSource
//
////////////////////////////////////////////////////////////////////////////////

FileBlockSource::~FileBlockSource() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockSource::ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockSource::ReadBlock (uint32_t block, std::span<Byte> outBytes)
{
    return m_file->GetStore().ReadBlocks (block, 1, outBytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockSource::WriteBlock
//
//  The block's content before its first write is read first, for a state
//  loaded from before that write.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockSource::WriteBlock (uint32_t block, std::span<const Byte> bytes)
{
    HRESULT  hr = S_OK;



    if (!m_original.contains (block))
    {
        std::vector<Byte>  before (BlockDeviceMedia::kBlockBytes);

        hr = m_file->GetStore().ReadBlocks (block, 1, before);
        CHR (hr);

        m_original[block] = std::move (before);
    }

    hr = m_file->GetStore().WriteBlocks (block, bytes);
    CHR (hr);

    m_hasPendingWrites = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockSource::RestoreWritten
//
//  A block written since the save gets its content from before its first
//  write; a block the save had written gets the saved content.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockSource::RestoreWritten (const WrittenBlocks & written)
{
    HRESULT  hr = S_OK;



    for (const auto & [changed, original] : m_original)
    {
        if (!written.contains (changed))
        {
            hr = m_file->GetStore().WriteBlocks (changed, original);
            CHR (hr);
        }
    }

    for (const auto & [saved, bytes] : written)
    {
        hr = m_file->GetStore().WriteBlocks (saved, *bytes);
        CHR (hr);
    }

    m_hasPendingWrites = m_hasPendingWrites || !m_original.empty() || !written.empty();

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockSource::Flush
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockSource::Flush()
{
    HRESULT  hr          = S_OK;
    bool     isUnchanged = false;



    BAIL_OUT_IF (!m_hasPendingWrites, S_OK);

    isUnchanged = m_file->IsFileUnchanged();
    CBREx (isUnchanged, HRESULT_FROM_WIN32 (ERROR_FILE_INVALID));

    hr = m_file->Commit();
    CHR (hr);

    m_hasPendingWrites = false;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockSource::HasChangedOnHost
//
////////////////////////////////////////////////////////////////////////////////

bool FileBlockSource::HasChangedOnHost()
{
    return !m_file->IsFileUnchanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  FileBlockSource::GetAttachedBlocks
//
//  The blocks now, with each written block's content before its first write
//  put back. Made once and shared by every copy after.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT FileBlockSource::GetAttachedBlocks (std::shared_ptr<const std::vector<Byte>> & outBlocks)
{
    constexpr size_t   kBlockBytes = BlockDeviceMedia::kBlockBytes;
    HRESULT            hr          = S_OK;
    uint32_t           count       = m_file->GetBlockCount();
    std::vector<Byte>  blocks;



    if (m_attachedBlocks == nullptr)
    {
        blocks.assign ((size_t) count * kBlockBytes, 0);

        hr = m_file->GetStore().ReadBlocks (0, count, blocks);
        CHR (hr);

        for (const auto & [block, original] : m_original)
        {
            std::copy (original.begin(), original.end(), blocks.begin() + (ptrdiff_t) ((size_t) block * kBlockBytes));
        }

        m_attachedBlocks = std::make_shared<const std::vector<Byte>> (std::move (blocks));
    }

    outBlocks = m_attachedBlocks;

Error:
    return hr;
}
