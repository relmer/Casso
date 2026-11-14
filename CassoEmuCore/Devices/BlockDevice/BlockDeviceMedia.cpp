#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/Disk/Containers/TwoImgContainer.h"
#include "Devices/Disk/IBlockStore.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceMedia
//
////////////////////////////////////////////////////////////////////////////////

BlockDeviceMedia::BlockDeviceMedia() = default;





////////////////////////////////////////////////////////////////////////////////
//
//  ~BlockDeviceMedia
//
////////////////////////////////////////////////////////////////////////////////

BlockDeviceMedia::~BlockDeviceMedia()
{
    HRESULT  hr = Detach();



    IGNORE_RETURN_VALUE (hr, S_OK);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Attach
//
//  Opens the image and checks it before claiming the path: a DOS-order 2IMG
//  file, an image larger than the drive, an empty image, a DiskCopy 4.2
//  image whose data checksum does not match, and a path attached to another
//  drive all fail, and nothing is written to the file.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::Attach (
    IDiskFileIo             & fileIo,
    const std::string       & path,
    uint32_t                  maxBlocks,
    const WriteProtectInfo  & hostProtect,
    std::wstring            & outError)
{
    HRESULT                          hr         = S_OK;
    std::unique_ptr<BlockImageFile>  file;
    std::string                      key        = NormalizePath (path);
    uint32_t                         blockCount = 0;
    bool                             isClaimed  = false;
    bool                             isSumValid = true;



    hr = Detach();
    CHR (hr);

    hr = BlockImageFile::Open (fileIo, path, file, outError);
    CHR (hr);

    blockCount = file->GetBlockCount();

    CBREx (blockCount > 0,          HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));
    CBREx (blockCount <= maxBlocks, HRESULT_FROM_WIN32 (ERROR_DISK_TOO_FRAGMENTED));

    hr = file->IsDataChecksumValid (isSumValid);
    CHR (hr);

    CBREx (isSumValid, HRESULT_FROM_WIN32 (ERROR_CRC));

    isClaimed = ChangeClaim (key, true);
    CBREx (isClaimed, HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION));

    m_protect           = hostProtect;
    m_protect.imageFlag = IsLockedTwoImg (*file);
    m_file              = std::move (file);
    m_path              = path;
    m_pathKey           = key;
    m_blockCount        = blockCount;
    m_hasPendingWrites  = false;

Error:
    if (hr == HRESULT_FROM_WIN32 (ERROR_DISK_TOO_FRAGMENTED))
    {
        outError = std::format (L"The image has {} blocks, and this drive takes at most {}.", blockCount, maxBlocks);
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT) && outError.empty())
    {
        outError = L"The image has no blocks.";
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_CRC))
    {
        outError = L"The image's Disk Copy checksum doesn't match its data, so the image is damaged.";
    }
    else if (hr == HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION))
    {
        outError = L"The image is attached to another drive.";
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Detach
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::Detach()
{
    HRESULT  hr       = S_OK;
    bool     released = false;



    BAIL_OUT_IF (m_file == nullptr, S_OK);

    hr = Flush();

Error:
    if (m_file != nullptr)
    {
        released = ChangeClaim (m_pathKey, false);
        IGNORE_RETURN_VALUE (released, false);

        m_file.reset();
        m_path.clear();
        m_pathKey.clear();
        m_blockCount       = 0;
        m_protect          = {};
        m_hasPendingWrites = false;
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadBlock
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::ReadBlock (uint32_t block, std::span<Byte> outBytes)
{
    HRESULT  hr     = S_OK;
    size_t   length = outBytes.size();



    CBRAEx (length == kBlockBytes, E_INVALIDARG);
    CBREx  (m_file != nullptr,              HRESULT_FROM_WIN32 (ERROR_NOT_READY));
    CBREx  (block < m_blockCount,           HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));

    hr = m_file->GetStore().ReadBlocks (block, 1, outBytes);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WriteBlock
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::WriteBlock (uint32_t block, std::span<const Byte> bytes)
{
    HRESULT  hr          = S_OK;
    bool     isProtected = m_protect.Any();
    size_t   length      = bytes.size();



    CBRAEx (length == kBlockBytes, E_INVALIDARG);
    CBREx  (m_file != nullptr,           HRESULT_FROM_WIN32 (ERROR_NOT_READY));
    CBREx  (block < m_blockCount,        HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));
    CBREx  (!isProtected,                HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT));

    hr = m_file->GetStore().WriteBlocks (block, bytes);
    CHR (hr);

    m_hasPendingWrites = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Flush
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::Flush()
{
    HRESULT  hr          = S_OK;
    bool     isUnchanged = false;



    BAIL_OUT_IF (m_file == nullptr || !m_hasPendingWrites, S_OK);

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
//  HasChangedOnHost
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceMedia::HasChangedOnHost()
{
    return m_file != nullptr && !m_file->IsFileUnchanged();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPathAttached
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceMedia::IsPathAttached (const std::string & path)
{
    std::string  key     = NormalizePath (path);
    bool         isFree  = ChangeClaim (key, true);



    if (isFree)
    {
        isFree = ChangeClaim (key, false);
        IGNORE_RETURN_VALUE (isFree, false);
        return false;
    }

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  NormalizePath
//
//  The key one-writer-per-path compares: lowercase, with forward slashes
//  turned to backslashes, since the host's file names ignore case.
//
////////////////////////////////////////////////////////////////////////////////

std::string BlockDeviceMedia::NormalizePath (const std::string & path)
{
    std::string  key = path;



    for (char & ch : key)
    {
        ch = (ch == '/') ? '\\' : (char) std::tolower ((unsigned char) ch);
    }

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangeClaim
//
//  Claims a path for a drive, returning false when another drive has it, or
//  releases it. The attached paths are shared by every machine in the
//  process, so the set is guarded.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceMedia::ChangeClaim (const std::string & key, bool isClaim)
{
    static std::mutex             s_lock;
    static std::set<std::string>  s_paths;
    std::lock_guard<std::mutex>   guard (s_lock);
    bool                          changed = false;



    if (isClaim)
    {
        changed = !key.empty() && s_paths.insert (key).second;
    }
    else
    {
        changed = s_paths.erase (key) > 0;
    }

    return changed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsLockedTwoImg
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceMedia::IsLockedTwoImg (const BlockImageFile & file)
{
    constexpr size_t    kFlagsOffset = 16;
    const auto        & header       = file.GetHeader();
    bool                isTwo        = file.GetKind() == BlockContainerKind::TwoImg && header.size() >= kFlagsOffset + sizeof (uint32_t);



    return isTwo && (BlockImageFile::ReadLittleLong (header, kFlagsOffset) & TwoImgHeader::kFlagLocked) != 0;
}
