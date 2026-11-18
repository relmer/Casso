#pragma once

#include "Pch.h"

#include "CassoExplorer/Model/CollectionWalk.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DuplicateSet
//
//  Disks with the same volume data, or files with the same data and type on
//  disks that differ. A member is a disk's index in the walk's list and,
//  for a file, its path on that disk.
//
////////////////////////////////////////////////////////////////////////////////

struct DuplicateMember
{
    size_t       disk = 0;
    std::string  file;
};

struct DuplicateSet
{
    bool                          isFile = false;
    std::string                   sha256;
    Byte                          type   = 0;
    std::vector<DuplicateMember>  members;
};





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionResults
//
//  Find duplicates over the disks a walk found, and the CSV text of Disk
//  list and of Find duplicates, which the window's Save as CSV and
//  `casso disk list` and `casso disk dupes` write.
//
//  The disk sets come first, then the file sets, each in the order of its
//  first member's path. A file set has members on disks whose volume data
//  differs, so the files of two copies of one disk don't form a set, and an
//  empty file isn't in any set.
//
////////////////////////////////////////////////////////////////////////////////

class CollectionResults
{
public:
    static void  FindDuplicates (const std::vector<CollectionDisk> & disks, std::vector<DuplicateSet> & outSets);

    static std::string  FormatDiskList   (const std::vector<CollectionDisk> & disks);
    static std::string  FormatDuplicates (const std::vector<CollectionDisk> & disks, const std::vector<DuplicateSet> & sets);

    //  One row of each, with its line end; the set is numbered from 1.
    static std::string  FormatDiskRow      (const CollectionDisk & disk);
    static std::string  FormatDuplicateRow (int set, const DuplicateSet & duplicates, const DuplicateMember & member, const std::vector<CollectionDisk> & disks);

    static constexpr const char *  kDiskListHeader   = "path,container,fileSystem,volumeName,files,freeBytes,crc32,sha256,hasMore\n";
    static constexpr const char *  kDuplicatesHeader = "set,kind,path,name,sha256\n";

    static std::string  QuoteField (const std::string & field);

    static constexpr const char *  kLineEnd = "\n";

private:
    static std::string  GetSortKey (const std::vector<CollectionDisk> & disks, const DuplicateSet & set);
};
