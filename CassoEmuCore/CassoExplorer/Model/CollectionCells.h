#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  CollectionCells
//
//  The columns of a Disk list or Find duplicates row: a disk's container,
//  file system, volume name, file count, free space and checksums, the
//  duplicate set the row is in, and the row's line of Save as CSV. They
//  are empty in a row of any other folder.
//
////////////////////////////////////////////////////////////////////////////////

struct CollectionCells
{
    bool          isDisk    = false;
    int           set       = 0;       // the duplicate set, from 1; 0 in a Disk list
    std::wstring  container;
    std::wstring  fileSystem;
    std::wstring  volumeName;
    uint32_t      files     = 0;
    uint64_t      freeBytes = 0;
    std::wstring  crc32;
    std::wstring  sha256;
    bool          hasMore   = false;
    std::string   csvLine;

    bool  operator== (const CollectionCells &) const = default;
};
