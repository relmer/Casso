#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskOperations.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"

class IDiskFileIo;
class IImageRecords;





////////////////////////////////////////////////////////////////////////////////
//
//  ImageSubdirectory
//
////////////////////////////////////////////////////////////////////////////////

struct ImageSubdirectory
{
    std::string  path;
    std::string  name;
    bool         hasSubdirectories = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ImageContents
//
////////////////////////////////////////////////////////////////////////////////

struct ImageContents
{
    DiskOperations::Result          result;
    VolumeKind                      kind   = VolumeKind::Unknown;
    VolumeListing                   listing;
    std::vector<ImageSubdirectory>  subdirectories;
    PartitionLayoutKind             layout = PartitionLayoutKind::None;
    std::vector<PartitionEntry>     partitions;
    std::wstring                    error;
    bool                            broken = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageCatalog
//
//  What a disk image, or a partition of one, contains, for the tree, the
//  list and the preview.
//
////////////////////////////////////////////////////////////////////////////////

class DiskImageCatalog
{
public:
    explicit DiskImageCatalog (DiskOperations & operations) : m_operations (operations) {}

    //  The stored records an image's volume directory is read from and
    //  stored to; none reads every image.
    void             SetRecords (IImageRecords * records) { m_records = records; }
    IImageRecords  * GetRecords () const                  { return m_records; }

    HRESULT  Describe       (const std::wstring & path, const std::string & directory, ImageContents & outContents);
    HRESULT  ListPartitions (const std::wstring & path, PartitionLayoutKind & outLayout, std::vector<PartitionEntry> & outEntries);

    static HRESULT       ListFilePartitions       (IDiskFileIo & fileIo, const std::wstring & path, PartitionLayoutKind & outLayout, std::vector<PartitionEntry> & outEntries);
    static std::wstring  GetPartitionLabel        (const std::wstring & imagePath, const PartitionEntry & entry, bool withSize);
    static std::wstring  GetPartitionTitle        (IDiskFileIo & fileIo, const std::wstring & path);
    static std::wstring  DescribeDamagedPartition (const std::wstring & imagePath, const PartitionEntry & entry);

private:
    std::wstring  DescribeFailure (const std::wstring & path, const std::string & message);

    static void  ListSubdirectories (const IVolume & volume, const std::string & directory, const VolumeListing & listing, std::vector<ImageSubdirectory> & outSubdirectories);

    DiskOperations  & m_operations;
    IImageRecords   * m_records = nullptr;
};
