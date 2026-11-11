#include "Pch.h"

#include "Devices/Disk/BlockVolumeOpener.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/BlockImage.h"
#include "Devices/Disk/FileBlockStore.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BlockVolumeOpener::TryOpen
//
////////////////////////////////////////////////////////////////////////////////

bool BlockVolumeOpener::TryOpen (IDiskFileIo                    & fileIo,
                                 const std::string              & imagePath,
                                 VolumeKind                       choice,
                                 std::unique_ptr<IBlockStore>   & outStore,
                                 std::unique_ptr<ProDosVolume>  & outVolume)
{
    HRESULT             hr       = S_OK;
    std::string         lower    = imagePath;
    bool                isCard   = PhysicalCards::IsOnCard (TextEncoding::NarrowToWide (imagePath));
    bool                isBlocks = false;
    bool                is2mg    = false;
    bool                isProDos = false;
    uint64_t            offset   = 0;
    uint64_t            length   = 0;
    FileStamp           stamp;
    std::vector<Byte>   header (s_k2mgHeaderBytes);



    outStore.reset();
    outVolume.reset();

    std::transform (lower.begin(), lower.end(), lower.begin(), [] (char c) { return (char) std::tolower ((unsigned char) c); });

    is2mg    = lower.ends_with (".2mg");
    isBlocks = lower.ends_with (".po") || lower.ends_with (".hdv") || is2mg;

    BAIL_OUT_IF (choice != VolumeKind::Unknown && choice != VolumeKind::ProDos, S_OK);
    BAIL_OUT_IF (!isBlocks || (lower.find ('|') != std::string::npos && !isCard), S_OK);

    hr = fileIo.Stat (imagePath, stamp);
    CHR (hr);

    length = stamp.sizeBytes;

    if (is2mg)
    {
        BAIL_OUT_IF (length < s_k2mgHeaderBytes, S_OK);

        hr = fileIo.ReadAt (imagePath, 0, header);
        CHR (hr);

        BAIL_OUT_IF (!std::equal (header.begin(), header.begin() + 4, "2IMG") || ReadLong (header, s_k2mgFormat) != s_k2mgProDosOrder, S_OK);

        offset = ReadLong (header, s_k2mgDataOffset);
        length = ReadLong (header, s_k2mgDataLength);

        BAIL_OUT_IF (offset + length > stamp.sizeBytes, S_OK);
    }

    BAIL_OUT_IF (length < kMinBytes, S_OK);

    outStore = std::make_unique<FileBlockStore> (fileIo, imagePath, offset, (uint32_t) std::min<uint64_t> (length / IBlockStore::kBlockBytes, UINT32_MAX));

    {
        BlockImage  image (*outStore);

        isProDos = (image[s_kVolumeHeaderAt] & 0xF0) == s_kVolumeHeaderType;

        if (isProDos)
        {
            outVolume = std::make_unique<ProDosVolume> (image);
        }
    }

Error:
    if (outVolume == nullptr)
    {
        outStore.reset();
    }

    return outVolume != nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BlockVolumeOpener::ReadLong
//
////////////////////////////////////////////////////////////////////////////////

uint32_t BlockVolumeOpener::ReadLong (const std::vector<Byte> & bytes, size_t at)
{
    return (uint32_t) bytes[at] | ((uint32_t) bytes[at + 1] << 8) | ((uint32_t) bytes[at + 2] << 16) | ((uint32_t) bytes[at + 3] << 24);
}
