#include "Pch.h"

#include "Core/StateReader.h"
#include "Core/StateWriter.h"
#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/BlockDevice/FileBlockSource.h"
#include "Devices/BlockDevice/MemoryBlockSource.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Floppy35/Gcr35Track.h"
#include "Machines/Apple2/Common/WozLoader.h"
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
//  IsNibbleImage
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceMedia::IsNibbleImage (const std::string & path)
{
    std::string  ext = std::filesystem::path (path).extension().string();



    std::transform (ext.begin(), ext.end(), ext.begin(), [] (char c) { return (char) std::tolower ((unsigned char) c); });

    return ext == ".nib" || ext == ".nb2";
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsFloppyBlockCount
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceMedia::IsFloppyBlockCount (uint32_t blockCount)
{
    constexpr uint32_t  kBlocks140K = 280;
    constexpr uint32_t  kBlocks400K = 800;
    constexpr uint32_t  kBlocks800K = 1600;



    return blockCount == kBlocks140K || blockCount == kBlocks400K || blockCount == kBlocks800K;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Attach
//
//  Opens the image and checks it before claiming the path: a DOS-order 2IMG
//  file, an image larger than the drive, an empty image, a DiskCopy 4.2
//  image whose data checksum does not match, and a path attached to another
//  drive all fail, and nothing is written to the file. So do a nibble image,
//  which is a 5.25-inch disk's raw track data and not blocks, and a raw image
//  that isn't a whole number of blocks, which is damaged: read as blocks, its
//  last partial block was dropped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::Attach (
    IDiskFileIo             & fileIo,
    const std::string       & path,
    uint32_t                  maxBlocks,
    const WriteProtectInfo  & hostProtect,
    std::wstring            & outError)
{
    constexpr size_t                 kSignatureBytes = 4;
    HRESULT                          hr              = S_OK;
    HRESULT                          probe           = S_OK;
    std::unique_ptr<BlockImageFile>  file;
    std::string                      key             = GetClaimKey (fileIo, path);
    uint32_t                         blockCount      = 0;
    bool                             isClaimed       = false;
    bool                             isSumValid      = true;
    bool                             isWoz           = false;
    bool                             isNibble        = IsNibbleImage (path);
    FileStamp                        stamp;
    std::vector<Byte>                head       (kSignatureBytes, 0);
    std::vector<Byte>                bytes;
    std::vector<Byte>                wozBlocks;



    hr = Detach();
    CHR (hr);

    probe = fileIo.ReadAt (path, 0, head);
    isWoz = SUCCEEDED (probe) && head[0] == 'W' && head[1] == 'O' && head[2] == 'Z';

    if (isWoz)
    {
        hr = fileIo.ReadAllBytes (path, bytes);
        CHR (hr);

        hr = DecodeWoz (bytes, wozBlocks, outError);
        CHR (hr);

        blockCount = (uint32_t) (wozBlocks.size() / kBlockBytes);
        CBREx (blockCount <= maxBlocks, HRESULT_FROM_WIN32 (ERROR_DISK_TOO_FRAGMENTED));

        isClaimed = ChangeClaim (key, true);
        CBREx (isClaimed, HRESULT_FROM_WIN32 (ERROR_SHARING_VIOLATION));

        m_protect           = hostProtect;
        m_protect.imageFlag = true;
        m_source            = std::make_unique<MemoryBlockSource> (std::make_shared<const std::vector<Byte>> (std::move (wozBlocks)));
        m_path              = path;
        m_pathKey           = key;
        m_blockCount        = blockCount;

        BAIL_OUT_IF (true, S_OK);
    }

    CBRF (!isNibble, outError = L"The image is a nibble image of a 5.25-inch disk, not a block image.");

    hr = BlockImageFile::Open (fileIo, path, file, outError);
    CHR (hr);

    if (file->GetKind() == BlockContainerKind::Raw)
    {
        hr = fileIo.Stat (path, stamp);
        CHR (hr);

        CBRF (stamp.sizeBytes % kBlockBytes == 0,
              outError = std::format (L"The image is {} bytes, which isn't a whole number of 512-byte blocks, so it's damaged.", stamp.sizeBytes));
    }

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
    m_source            = std::make_unique<FileBlockSource> (std::move (file));
    m_path              = path;
    m_pathKey           = key;
    m_blockCount        = blockCount;

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
//  DecodeWoz
//
//  A WOZ image of a 3.5-inch disk whose every block decodes. The error text
//  lists the first blocks that don't.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::DecodeWoz (const std::vector<Byte> & bytes, std::vector<Byte> & outBlocks, std::wstring & outError)
{
    constexpr size_t        kListed  = 8;
    HRESULT                 hr       = S_OK;
    WozLoader::Description  about;
    DiskImage               image;
    std::vector<uint32_t>   missing;
    std::wstring            list;
    bool                    isLoaded = false;



    WozLoader::Describe (bytes, about);

    if (!about.isWoz || about.diskType != WozLoader::kDiskType35)
    {
        outError = L"The WOZ image isn't a 3.5-inch disk.";
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));
    }

    image.LoadFromBytes (DiskFormat::Woz, bytes, "");
    isLoaded = image.IsLoaded();

    if (!isLoaded)
    {
        outError = L"The WOZ image's tracks don't load.";
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_BAD_FORMAT));
    }

    Gcr35Track::DecodeWoz (image, (about.info.sides == 1) ? 1 : 2, outBlocks, missing);

    if (!missing.empty())
    {
        for (size_t i = 0; i < missing.size() && i < kListed; i++)
        {
            list += (i == 0) ? std::format (L"{}", missing[i]) : std::format (L", {}", missing[i]);
        }

        outError = std::format (L"{} of the WOZ image's blocks don't decode: {}{}.", missing.size(), list, missing.size() > kListed ? L" and more" : L"");
        CBREx (false, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT));
    }

Error:
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



    bool     attached = IsAttached();



    BAIL_OUT_IF (!attached, S_OK);

    hr = Flush();

Error:
    if (attached)
    {
        released = ChangeClaim (m_pathKey, false);
        IGNORE_RETURN_VALUE (released, false);

        m_source.reset();
        m_path.clear();
        m_pathKey.clear();
        m_blockCount = 0;
        m_protect    = {};
        m_written.clear();
    }

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ReadBlock
//
//  A written block from the overlay, any other from the source.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::ReadBlock (uint32_t block, std::span<Byte> outBytes)
{
    HRESULT  hr       = S_OK;
    size_t   length   = outBytes.size();
    bool     attached = IsAttached();
    auto     written  = m_written.end();



    CBRAEx (length == kBlockBytes, E_INVALIDARG);
    CBREx  (attached,              HRESULT_FROM_WIN32 (ERROR_NOT_READY));
    CBREx  (block < m_blockCount,  HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));

    written = m_written.find (block);

    if (written != m_written.end())
    {
        std::copy_n (written->second->begin(), kBlockBytes, outBytes.begin());
        BAIL_OUT_IF (true, S_OK);
    }

    hr = m_source->ReadBlock (block, outBytes);
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
    bool     attached    = IsAttached();
    size_t   length      = bytes.size();



    CBRAEx (length == kBlockBytes, E_INVALIDARG);
    CBREx  (attached,                    HRESULT_FROM_WIN32 (ERROR_NOT_READY));
    CBREx  (block < m_blockCount,        HRESULT_FROM_WIN32 (ERROR_SECTOR_NOT_FOUND));
    CBREx  (!isProtected,                HRESULT_FROM_WIN32 (ERROR_WRITE_PROTECT));

    hr = m_source->WriteBlock (block, bytes);
    CHR (hr);

    m_written[block] = std::make_shared<const std::vector<Byte>> (bytes.begin(), bytes.end());

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
    return (m_source != nullptr) ? m_source->Flush() : S_OK;
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasChangedOnHost
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceMedia::HasChangedOnHost()
{
    return m_source != nullptr && m_source->HasChangedOnHost();
}





////////////////////////////////////////////////////////////////////////////////
//
//  HasPendingWrites
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceMedia::HasPendingWrites() const
{
    return m_source != nullptr && m_source->HasPendingWrites();
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsPathAttached
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceMedia::IsPathAttached (const IDiskFileIo & fileIo, const std::string & path)
{
    std::string  key     = GetClaimKey (fileIo, path);
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
//  GetClaimKey
//
//  The path, and the file system it is on unless that is the host's: the
//  same path on two file systems in memory is two files.
//
////////////////////////////////////////////////////////////////////////////////

std::string BlockDeviceMedia::GetClaimKey (const IDiskFileIo & fileIo, const std::string & path)
{
    std::string  key      = NormalizePath (path);
    uintptr_t    systemId = fileIo.GetFileSystemId();



    if (systemId != 0)
    {
        key = std::format ("{:x}|{}", systemId, key);
    }

    return key;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ChangeClaim
//
//  Marks a path as attached to a drive, returning false when another drive has it, or
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





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceMedia::SaveState
//
//  The media's path and size, then each written block and its content.
//
////////////////////////////////////////////////////////////////////////////////

void BlockDeviceMedia::SaveState (StateWriter & writer) const
{
    writer.WriteUInt32 (static_cast<uint32_t> (m_path.size()));
    writer.WriteBytes  (reinterpret_cast<const Byte *> (m_path.data()), m_path.size());
    writer.WriteUInt32 (m_blockCount);
    writer.WriteUInt32 (static_cast<uint32_t> (m_written.size()));

    for (const auto & [block, content] : m_written)
    {
        writer.WriteUInt32 (block);
        writer.WriteShared (content, content->data());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceMedia::LoadState
//
//  A block written since the save gets its content from before its first
//  write; a block the save had written gets the saved content. Media other
//  than the saved media, by path or by size, is bad data.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::LoadState (StateReader & reader)
{
    HRESULT                                                       hr         = S_OK;
    uint32_t                                                      pathLength = 0;
    uint32_t                                                      blockCount = 0;
    uint32_t                                                      count      = 0;
    uint32_t                                                      block      = 0;
    std::string                                                   path;
    std::vector<Byte>                                             content;
    std::map<uint32_t, std::shared_ptr<const std::vector<Byte>>>  written;



    reader.ReadUInt32 (pathLength);
    CBREx (pathLength <= MAX_PATH * 4, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    path.resize (pathLength);
    reader.ReadBytes  (reinterpret_cast<Byte *> (path.data()), pathLength);
    reader.ReadUInt32 (blockCount);
    reader.ReadUInt32 (count);

    hr = reader.GetResult();
    CHR (hr);

    CBREx (path == m_path && blockCount == m_blockCount && count <= m_blockCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    for (uint32_t i = 0; i < count; i++)
    {
        content.assign (kBlockBytes, 0);

        reader.ReadUInt32 (block);
        reader.ReadBytes  (content.data(), content.size());

        hr = reader.GetResult();
        CHR (hr);

        CBREx (block < m_blockCount, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

        written[block] = std::make_shared<const std::vector<Byte>> (std::move (content));
    }

    if (m_source != nullptr)
    {
        hr = m_source->RestoreWritten (written);
        CHR (hr);
    }

    m_written = std::move (written);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceMedia::CopyForReplay
//
//  The blocks as they were when the media was attached. An empty drive's
//  copy has no content.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::CopyForReplay (ReplayBlockMedia & outCopy)
{
    HRESULT  hr = S_OK;



    outCopy = {};

    BAIL_OUT_IF (!IsAttached(), S_OK);

    hr = m_source->GetAttachedBlocks (outCopy.content);
    CHR (hr);

    outCopy.path    = m_path;
    outCopy.protect = m_protect;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockDeviceMedia::MountReplayCopy
//
//  After a copy with no content, the drive is empty.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT BlockDeviceMedia::MountReplayCopy (const ReplayBlockMedia & copy)
{
    HRESULT  hr      = S_OK;
    size_t   size    = 0;
    bool     isWhole = false;



    hr = Detach();
    CHR (hr);

    BAIL_OUT_IF (copy.content == nullptr, S_OK);

    size    = copy.content->size();
    isWhole = size > 0 && size % kBlockBytes == 0;
    CBRAEx (isWhole, E_INVALIDARG);

    m_source     = std::make_unique<MemoryBlockSource> (copy.content);
    m_path       = copy.path;
    m_blockCount = (uint32_t) (size / kBlockBytes);
    m_protect    = copy.protect;

Error:
    return hr;
}
