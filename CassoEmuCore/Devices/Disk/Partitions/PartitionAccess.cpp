#include "Pch.h"

#include "Devices/Disk/Partitions/PartitionAccess.h"
#include "Devices/Disk/Archives/GzipFile.h"
#include "Devices/Disk/Containers/ImageContainer.h"
#include "Devices/Disk/MemoryBlockStore.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::List
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::List (const std::vector<Byte>      & fileBytes,
                               const std::string            & name,
                               PartitionLayoutKind          & outKind,
                               std::vector<PartitionEntry>  & outEntries)
{
    static constexpr size_t  kGzipExtension = 3;



    HRESULT                    hr        = S_OK;
    bool                       isGzip    = GzipFile::IsGzip (fileBytes);
    bool                       isRaw     = false;
    const std::vector<Byte>  * source    = &fileBytes;
    std::string                innerName = name;
    std::vector<Byte>          expanded;
    std::vector<Byte>          blocks;
    std::string                gzipName;
    std::string                error;



    outKind = PartitionLayoutKind::None;
    outEntries.clear();

    if (isGzip)
    {
        hr = GzipFile::Expand (fileBytes, expanded, gzipName, error);
        CHR (hr);

        source = &expanded;

        if (innerName.size() > kGzipExtension)
        {
            innerName.resize (innerName.size() - kGzipExtension);
        }
    }

    hr = OpenBlocks (*source, innerName, blocks, isRaw);
    CHR (hr);

    {
        MemoryBlockStore  store (std::move (blocks));

        hr = ListStore (store, outKind, outEntries);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::ListStore
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::ListStore (IBlockStore & store, PartitionLayoutKind & outKind, std::vector<PartitionEntry> & outEntries)
{
    HRESULT                 hr = S_OK;
    std::vector<Partition>  partitions;



    outKind = PartitionLayoutKind::None;
    outEntries.clear();

    hr = PartitionLayout::Detect (store, outKind, partitions);
    CHR (hr);

    for (const Partition & partition : partitions)
    {
        PartitionEntry  entry;

        entry.partition = partition;
        entry.step      = GetStepName (partition);
        entry.byteCount = GetByteCount (partition);

        ReadVolumeLabel (store, entry);

        outEntries.push_back (std::move (entry));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::ListCard
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::ListCard (IBlockStore & store, PartitionLayoutKind & outKind, std::vector<PartitionEntry> & outEntries)
{
    HRESULT                 hr         = S_OK;
    uint32_t                blocks     = store.GetBlockCount();
    bool                    oneVolume  = false;
    std::vector<Partition>  partitions;
    PartitionEntry          entry;



    hr = ListStore (store, outKind, outEntries);
    CHR (hr);

    BAIL_OUT_IF (outKind != PartitionLayoutKind::None || blocks == 0 || blocks > kCffaSliceBlocks, S_OK);

    hr = PartitionLayout::AppendBlockPartition (store, PartitionLayoutKind::Cffa, 0, blocks, "", partitions);
    CHR (hr);

    oneVolume = partitions.back().volumeKind == VolumeKind::ProDos || partitions.back().isHfs;
    BAIL_OUT_IF (!oneVolume, S_OK);

    partitions.back().index = 1;

    entry.partition = partitions.back();
    entry.step      = GetStepName (entry.partition);
    entry.byteCount = GetByteCount (entry.partition);

    ReadVolumeLabel (store, entry);

    outKind = PartitionLayoutKind::Cffa;
    outEntries.push_back (std::move (entry));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::GetStepName
//
////////////////////////////////////////////////////////////////////////////////

std::string PartitionAccess::GetStepName (const Partition & partition)
{
    bool  isSectors = partition.dosSectors || partition.span != PartitionSpan::WholeBlocks;



    return std::format ("#{}.{}", partition.index, isSectors ? "do" : "po");
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::TryParseStepName
//
//  `#`, the partition's number in decimal, and `.po` or `.do` in any case.
//
////////////////////////////////////////////////////////////////////////////////

bool PartitionAccess::TryParseStepName (const std::string & step, int & outIndex)
{
    static constexpr size_t  kExtension = 3;
    static constexpr size_t  kMaxDigits = 4;



    size_t  digits = (step.size() > kExtension + 1) ? step.size() - kExtension - 1 : 0;
    size_t  i      = 0;
    int     index  = 0;
    bool    isPo   = false;
    bool    isDo   = false;



    outIndex = 0;

    if (digits == 0 || digits > kMaxDigits || step[0] != '#')
    {
        return false;
    }

    for (i = 1; i <= digits; i++)
    {
        if (step[i] < '0' || step[i] > '9')
        {
            return false;
        }

        index = index * 10 + (step[i] - '0');
    }

    isPo = _stricmp (step.c_str() + digits + 1, ".po") == 0;
    isDo = _stricmp (step.c_str() + digits + 1, ".do") == 0;

    if (index == 0 || (!isPo && !isDo))
    {
        return false;
    }

    outIndex = index;

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::GetByteCount
//
////////////////////////////////////////////////////////////////////////////////

uint64_t PartitionAccess::GetByteCount (const Partition & partition)
{
    uint64_t  unitBytes = (partition.span == PartitionSpan::WholeBlocks) ? IBlockStore::kBlockBytes : IBlockStore::kBlockBytes / 2;



    return (uint64_t) partition.blockCount * unitBytes;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::Extract
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::Extract (const std::vector<Byte>  & fileBytes,
                                  const std::string        & name,
                                  int                        index,
                                  std::vector<Byte>        & outPartitionBytes,
                                  std::string              & outError)
{
    HRESULT            hr    = S_OK;
    bool               isRaw = false;
    std::vector<Byte>  blocks;
    Partition          partition;



    outPartitionBytes.clear();

    hr = OpenBlocks (fileBytes, name, blocks, isRaw);
    CHRF (hr, outError = std::format ("{} doesn't open as a disk image.", GetLeafName (name)));

    {
        MemoryBlockStore  store (std::move (blocks));

        hr = FindPartition (store, name, index, partition, outError);
        CHR (hr);

        hr = ReadBytes (store, partition, outPartitionBytes);
        CHRF (hr, outError = std::format ("Partition {} of {} couldn't be read.", index, GetLeafName (name)));
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::Replace
//
//  A file loaded as a disk image is saved through VolumeImage, in the form
//  it was loaded from. The bytes after the last whole block of a block image
//  are written back as they were.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::Replace (const std::vector<Byte>  & fileBytes,
                                  const std::string        & name,
                                  int                        index,
                                  std::span<const Byte>      partitionBytes,
                                  std::vector<Byte>        & outFileBytes,
                                  std::string              & outError)
{
    HRESULT            hr    = S_OK;
    bool               isRaw = false;
    bool               fits  = false;
    std::vector<Byte>  blocks;
    std::vector<Byte>  current;
    std::vector<Byte>  sectors;
    Partition          partition;
    std::string        leaf  = GetLeafName (name);
    std::string        reason;



    outFileBytes.clear();

    hr = OpenBlocks (fileBytes, name, blocks, isRaw);
    CHRF (hr, outError = std::format ("{} doesn't open as a disk image.", leaf));

    {
        MemoryBlockStore  store (std::move (blocks));

        hr = FindPartition (store, name, index, partition, outError);
        CHR (hr);

        fits = partitionBytes.size() <= GetByteCount (partition);
        CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), outError = std::format ("The image is larger than partition {} of {}.", index, leaf));

        hr = ReadBytes (store, partition, current);
        CHRF (hr, outError = std::format ("Partition {} of {} couldn't be read.", index, leaf));

        std::copy (partitionBytes.begin(), partitionBytes.end(), current.begin());

        hr = WriteBytes (store, partition, current);
        CHRF (hr, outError = std::format ("Partition {} of {} couldn't be written.", index, leaf));

        if (isRaw)
        {
            outFileBytes = store.GetBytes();
            outFileBytes.insert (outFileBytes.end(), fileBytes.begin() + (ptrdiff_t) outFileBytes.size(), fileBytes.end());
        }
        else
        {
            if (store.GetBytes().size() == kFloppyBytes)
            {
                VolumeImage::ProDosFileToDosLogical (store.GetBytes(), sectors);
            }
            else
            {
                sectors = store.GetBytes();
            }

            hr = VolumeImage::Save (fileBytes, name, sectors, outFileBytes, reason);
            CHRF (hr, outError = reason);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::ConvertImage
//
//  A 5.25" image is loaded in DOS logical order, which is the order of a DOS
//  partition's bytes. For a partition of blocks it is put in block order.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::ConvertImage (const Partition          & partition,
                                       const std::vector<Byte>  & imageBytes,
                                       const std::string        & imageName,
                                       std::vector<Byte>        & outPartitionBytes,
                                       std::string              & outError)
{
    HRESULT             hr        = S_OK;
    bool                isSectors = partition.dosSectors || partition.span != PartitionSpan::WholeBlocks;
    bool                fits      = false;
    std::vector<Byte>   sectors;
    SectorDecodeReport  report;
    std::string         leaf      = GetLeafName (imageName);



    outPartitionBytes.clear();

    hr = VolumeImage::Load (imageBytes, imageName, sectors, report);
    CHRF (hr, outError = std::format ("{} doesn't open as a disk image.", leaf));

    fits = sectors.size() <= GetByteCount (partition);
    CBRFEx (fits, HRESULT_FROM_WIN32 (ERROR_BAD_LENGTH), outError = std::format ("{} is larger than partition {}.", leaf, partition.index));

    if (!isSectors && sectors.size() == kFloppyBytes)
    {
        VolumeImage::DosLogicalToProDosFile (sectors, outPartitionBytes);
    }
    else
    {
        outPartitionBytes = std::move (sectors);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::OpenBlocks
//
//  A card image can be larger than any volume, so a block image larger than
//  a 5.25" disk isn't loaded as a volume: its bytes are the blocks.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::OpenBlocks (const std::vector<Byte> & fileBytes, const std::string & name, std::vector<Byte> & outBlocks, bool & outIsRaw)
{
    HRESULT             hr          = S_OK;
    ImageContainerKind  container   = ImageContainer::GetKind (name, fileBytes.size());
    size_t              wholeBlocks = fileBytes.size() / IBlockStore::kBlockBytes;
    std::vector<Byte>   sectors;
    SectorDecodeReport  report;



    outBlocks.clear();
    outIsRaw = container == ImageContainerKind::Blocks && fileBytes.size() > kFloppyBytes;

    if (outIsRaw)
    {
        outBlocks.assign (fileBytes.begin(), fileBytes.begin() + (ptrdiff_t) (wholeBlocks * IBlockStore::kBlockBytes));
    }
    else
    {
        hr = VolumeImage::Load (fileBytes, name, sectors, report);
        CHR (hr);

        if (sectors.size() == kFloppyBytes)
        {
            VolumeImage::DosLogicalToProDosFile (sectors, outBlocks);
        }
        else
        {
            outBlocks = std::move (sectors);
        }
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::FindPartition
//
//  A partition the map has ending past the end of the store is not opened.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::FindPartition (IBlockStore        & store,
                                        const std::string  & name,
                                        int                  index,
                                        Partition          & outPartition,
                                        std::string        & outError)
{
    HRESULT                 hr     = S_OK;
    bool                    found  = false;
    PartitionLayoutKind     kind   = PartitionLayoutKind::None;
    std::vector<Partition>  partitions;
    std::string             leaf   = GetLeafName (name);



    outPartition = {};

    hr = PartitionLayout::Detect (store, kind, partitions);
    CHRF (hr, outError = std::format ("The partitions of {} couldn't be read.", leaf));

    for (const Partition & partition : partitions)
    {
        if (!found && partition.index == index)
        {
            found        = true;
            outPartition = partition;
        }
    }

    CBRFEx (found, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND), outError = std::format ("{} has no partition {}.", leaf, index));
    CBRFEx (!outPartition.damaged, HRESULT_FROM_WIN32 (ERROR_FILE_CORRUPT), outError = std::format ("Partition {} ends past the end of {}.", index, leaf));

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::ReadBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::ReadBytes (IBlockStore & store, const Partition & partition, std::vector<Byte> & outBytes)
{
    HRESULT  hr = S_OK;



    if (partition.span == PartitionSpan::WholeBlocks)
    {
        hr = PartitionLayout::ReadBlocks (store, partition.firstBlock, partition.blockCount, outBytes);
        CHR (hr);
    }
    else
    {
        hr = PartitionLayout::ReadPartition (store, partition, outBytes);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::WriteBytes
//
////////////////////////////////////////////////////////////////////////////////

HRESULT PartitionAccess::WriteBytes (IBlockStore & store, const Partition & partition, const std::vector<Byte> & bytes)
{
    HRESULT  hr = S_OK;



    if (partition.span == PartitionSpan::WholeBlocks)
    {
        hr = store.WriteBlocks (partition.firstBlock, bytes);
        CHR (hr);
    }
    else
    {
        hr = PartitionLayout::WritePartition (store, partition, bytes);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::ReadVolumeLabel
//
//  The volume name or number of a partition with a file system Casso reads.
//  A volume that fails to list has neither.
//
////////////////////////////////////////////////////////////////////////////////

void PartitionAccess::ReadVolumeLabel (IBlockStore & store, PartitionEntry & ioEntry)
{
    HRESULT                   hr       = S_OK;
    bool                      readable = !ioEntry.partition.damaged && ioEntry.partition.volumeKind != VolumeKind::Unknown;
    std::vector<Byte>         sectors;
    std::unique_ptr<IVolume>  volume;
    VolumeListing             listing;



    BAIL_OUT_IF (!readable, S_OK);

    hr = PartitionLayout::ReadPartition (store, ioEntry.partition, sectors);
    CHR (hr);

    volume = VolumeImage::CreateVolume (ioEntry.partition.volumeKind, sectors);
    BAIL_OUT_IF (volume == nullptr, S_OK);

    hr = volume->Enumerate (listing);
    CHR (hr);

    if (listing.hasVolumeName)
    {
        ioEntry.volumeName = listing.volumeName;
    }

    ioEntry.hasVolumeNumber = listing.hasVolumeNumber;
    ioEntry.volumeNumber    = listing.volumeNumber;

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::GetLabel
//
////////////////////////////////////////////////////////////////////////////////

std::string PartitionAccess::GetLabel (const PartitionEntry & entry, bool withSize)
{
    std::string  label  = std::format ("Partition {}", entry.partition.index);
    std::string  volume = entry.volumeName.empty() ? entry.partition.name : entry.volumeName;



    if (volume.empty() && entry.hasVolumeNumber)
    {
        volume = std::format ("volume {}", entry.volumeNumber);
    }

    if (!volume.empty())
    {
        label += ": " + volume;
    }

    if (withSize)
    {
        label += " (" + FormatSize (entry.byteCount) + ")";
    }

    return label;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::FormatSize
//
//  Kilobytes below a megabyte, and megabytes to one decimal place above it,
//  with a zero decimal left off.
//
////////////////////////////////////////////////////////////////////////////////

std::string PartitionAccess::FormatSize (uint64_t byteCount)
{
    static constexpr uint64_t  kKilobyte = 1024;
    static constexpr uint64_t  kMegabyte = 1024 * 1024;



    uint64_t  tenths = (byteCount * 10 + kMegabyte / 2) / kMegabyte;



    if (byteCount < kMegabyte)
    {
        return std::format ("{} KB", (byteCount + kKilobyte - 1) / kKilobyte);
    }

    if (tenths % 10 == 0)
    {
        return std::format ("{} MB", tenths / 10);
    }

    return std::format ("{}.{} MB", tenths / 10, tenths % 10);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PartitionAccess::GetLeafName
//
//  The part of a path after its last separator, '/' or '\\'.
//
////////////////////////////////////////////////////////////////////////////////

std::string PartitionAccess::GetLeafName (const std::string & path)
{
    size_t  slash = path.find_last_of ("/\\");



    return (slash == std::string::npos) ? path : path.substr (slash + 1);
}
