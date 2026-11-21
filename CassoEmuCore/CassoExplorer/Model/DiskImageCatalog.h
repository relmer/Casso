#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/DiskOperations.h"
#include "Devices/Disk/Partitions/PartitionAccess.h"





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

    HRESULT  Describe       (const std::wstring & path, const std::string & directory, ImageContents & outContents);
    HRESULT  ListPartitions (const std::wstring & path, PartitionLayoutKind & outLayout, std::vector<PartitionEntry> & outEntries);

    static std::wstring  GetPartitionLabel        (const std::wstring & imagePath, const PartitionEntry & entry, bool withSize);
    static std::wstring  DescribeDamagedPartition (const std::wstring & imagePath, const PartitionEntry & entry);

private:
    std::wstring  DescribeFailure (const std::wstring & path, const std::string & message);

    static void  ListSubdirectories (const IVolume & volume, const std::string & directory, const VolumeListing & listing, std::vector<ImageSubdirectory> & outSubdirectories);

    DiskOperations  & m_operations;
};
