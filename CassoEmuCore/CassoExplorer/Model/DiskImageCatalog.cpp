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
        outContents.error  = DescribeFailure (path, outContents.result.message);
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
    return ListFilePartitions (m_operations.GetFileIo(), path, outLayout, outEntries);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageCatalog::ListFilePartitions
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskImageCatalog::ListFilePartitions (IDiskFileIo & fileIo, const std::wstring & path, PartitionLayoutKind & outLayout, std::vector<PartitionEntry> & outEntries)
{
    HRESULT            hr     = S_OK;
    std::string        narrow = TextEncoding::WideToNarrow (path);
    std::vector<Byte>  bytes;



    outLayout = PartitionLayoutKind::None;
    outEntries.clear();

    hr = fileIo.ReadAllBytes (narrow, bytes);
    CHR (hr);

    hr = PartitionAccess::List (bytes, narrow, outLayout, outEntries);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageCatalog::DescribeFailure
//
//  The sentence for an image that didn't open. For a partition of an image
//  file, the sentence starts with its label, and for one that ends past the
//  end of the image, it is the damaged partition's sentence, since reading it
//  fails for that reason.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskImageCatalog::DescribeFailure (const std::wstring & path, const std::string & message)
{
    HRESULT                      hr     = S_OK;
    size_t                       bar    = path.rfind (L'|');
    std::wstring                 parent = (bar == std::wstring::npos) ? std::wstring() : path.substr (0, bar);
    std::string                  step   = (bar == std::wstring::npos) ? std::string() : TextEncoding::WideToNarrow (path.substr (bar + 1));
    int                          index  = 0;
    PartitionLayoutKind          layout = PartitionLayoutKind::None;
    std::vector<PartitionEntry>  entries;



    if (parent.empty() || PhysicalCards::IsOnCard (parent) || !PartitionAccess::TryParseStepName (step, index))
    {
        return DiskMessageText::FormatImageError (path, message);
    }

    hr = ListPartitions (parent, layout, entries);

    for (const PartitionEntry & entry : SUCCEEDED (hr) ? entries : std::vector<PartitionEntry>())
    {
        if (entry.step == step)
        {
            return entry.partition.damaged ? DescribeDamagedPartition (parent, entry)
                                           : DiskMessageText::FormatImageError (path, message, GetPartitionLabel (parent, entry, false));
        }
    }

    return DiskMessageText::FormatImageError (path, message);
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
//  DiskImageCatalog::GetPartitionTitle
//
//  The label of the partition of an image file at `path`, such as
//  "Partitioned.hdv|#2.po", or empty for any other path.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DiskImageCatalog::GetPartitionTitle (IDiskFileIo & fileIo, const std::wstring & path)
{
    HRESULT                      hr     = S_OK;
    size_t                       bar    = path.rfind (L'|');
    std::wstring                 parent = (bar == std::wstring::npos) ? std::wstring() : path.substr (0, bar);
    std::string                  step   = (bar == std::wstring::npos) ? std::string() : TextEncoding::WideToNarrow (path.substr (bar + 1));
    int                          index  = 0;
    PartitionLayoutKind          layout = PartitionLayoutKind::None;
    std::vector<PartitionEntry>  entries;
    std::wstring                 title;



    if (parent.empty() || PhysicalCards::IsOnCard (parent) || !PartitionAccess::TryParseStepName (step, index))
    {
        return title;
    }

    hr = ListFilePartitions (fileIo, parent, layout, entries);

    for (const PartitionEntry & entry : SUCCEEDED (hr) ? entries : std::vector<PartitionEntry>())
    {
        if (title.empty() && entry.step == step)
        {
            title = GetPartitionLabel (parent, entry, false);
        }
    }

    return title;
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
