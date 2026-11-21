#include "Pch.h"

#include "CassoExplorer/Model/DiskImageCatalog.h"
#include "CassoExplorer/Model/DiskMessageText.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Devices/Disk/Partitions/PhysicalCards.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageCatalog::Describe
//
//  Reads the image once. `directory` is listed, with each subdirectory in it
//  and whether that one has subdirectories. For the volume directory, the
//  image's partitions are listed too, from the same read: an image with no
//  file system can have partitions, and so can a volume, such as DOS
//  Master's. A failure has the sentence for it in outContents.error, and
//  `broken` is true for a file that is not a disk image at all, as against
//  one with no file system Casso reads.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageCatalog::Describe (const std::wstring & path, const std::string & directory, ImageContents & outContents)
{
    HRESULT                         hr     = S_OK;
    HRESULT                         hrList = S_OK;
    std::string                     narrow = TextEncoding::WideToNarrow (path);
    DiskOperations::ListingVolume   opened;



    outContents        = ImageContents();
    outContents.result = m_operations.OpenForListing (narrow, opened);
    outContents.kind   = opened.kind;

    if (outContents.result.Succeeded())
    {
        hrList = opened.volume->EnumerateDirectory (FilePath::Parse (directory), outContents.listing);

        outContents.result.listingDamaged = !outContents.listing.damage.empty();

        if (FAILED (hrList))
        {
            outContents.result.hr      = hrList;
            outContents.result.message = DiskCommandResult::FormatFailure (narrow, "", "catalog could not be read");
        }
        else
        {
            ListSubdirectories (*opened.volume, directory, outContents.listing, outContents.subdirectories);
        }
    }

    if (directory.empty() && opened.store != nullptr)
    {
        hrList = PartitionAccess::ListStore (*opened.store, outContents.layout, outContents.partitions);
        IGNORE_RETURN_VALUE (hrList, S_OK);
    }
    else if (directory.empty() && !opened.opened.fileBytes.empty())
    {
        hrList = PartitionAccess::List (opened.opened.fileBytes, narrow, outContents.layout, outContents.partitions);
        IGNORE_RETURN_VALUE (hrList, S_OK);
    }

    if (!outContents.result.Succeeded())
    {
        outContents.error  = DiskMessageText::FormatImageError (path, outContents.result.message);
        outContents.broken = outContents.kind == VolumeKind::Unknown && outContents.partitions.empty() &&
                             outContents.result.message.find (DiskImageSession::kNoFilesystemText) == std::string::npos;
    }

    hr = outContents.result.hr;

    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageCatalog::ListPartitions
//
//  The partitions of an image file, or of a partition of one. A storage card
//  is listed through PhysicalCards, not here.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageCatalog::ListPartitions (const std::wstring & path, PartitionLayoutKind & outLayout, std::vector<PartitionEntry> & outEntries)
{
    HRESULT            hr     = S_OK;
    std::string        narrow = TextEncoding::WideToNarrow (path);
    std::vector<Byte>  bytes;



    outLayout = PartitionLayoutKind::None;
    outEntries.clear();

    hr = m_operations.GetFileIo().ReadAllBytes (narrow, bytes);
    CHR (hr);

    hr = PartitionAccess::List (bytes, narrow, outLayout, outEntries);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageCatalog::GetPartitionLabel
//
//  A card's partition is labeled by its number and volume, as "Partition 2:
//  GAMES", since a card's map doesn't label its partitions. Any other
//  partition is labeled by its map's name and type.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskImageCatalog::GetPartitionLabel (const std::wstring & imagePath, const PartitionEntry & entry, bool withSize)
{
    return PhysicalCards::IsOnCard (imagePath) ? TextEncoding::NarrowToWide (PartitionAccess::GetLabel (entry, withSize))
                                               : TextEncoding::NarrowToWide (PartitionAccess::GetPartitionLabel (entry.partition));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageCatalog::DescribeDamagedPartition
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskImageCatalog::DescribeDamagedPartition (const std::wstring & imagePath, const PartitionEntry & entry)
{
    return GetPartitionLabel (imagePath, entry, false) + L" " + DiskMessageText::kDamagedPartitionText;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageCatalog::ListSubdirectories
//
//  Each subdirectory is listed one level deeper, so only a directory that
//  contains subdirectories is marked as having them. A directory whose
//  listing fails is marked as having no subdirectories.
//
////////////////////////////////////////////////////////////////////////////////

void DiskImageCatalog::ListSubdirectories (
    const IVolume                   & volume,
    const std::string               & directory,
    const VolumeListing             & listing,
    std::vector<ImageSubdirectory>  & outSubdirectories)
{
    outSubdirectories.clear();

    for (const FileEntry & entry : listing.entries)
    {
        ImageSubdirectory  subdirectory;
        VolumeListing      below;
        HRESULT            hr = S_OK;

        if (!entry.isDirectory)
        {
            continue;
        }

        subdirectory.name = entry.name;
        subdirectory.path = directory.empty() ? entry.name : directory + "/" + entry.name;

        hr = volume.EnumerateDirectory (FilePath::Parse (subdirectory.path), below);

        for (const FileEntry & child : below.entries)
        {
            subdirectory.hasSubdirectories = subdirectory.hasSubdirectories || (SUCCEEDED (hr) && child.isDirectory);
        }

        outSubdirectories.push_back (std::move (subdirectory));
    }
}
