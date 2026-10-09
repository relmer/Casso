#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskCopy.h"
#include "Devices/Disk/Inspector/Finding.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ImageDetails
//
//  What the image file itself holds, for the Image tab (FR-050): its format,
//  size and read-only attribute, and for a WOZ every INFO field, every META
//  entry, the maps and record table as stored, and the problems the file has
//  (FR-051). Built from a DiskCopy alone, so it describes the file as read.
//
////////////////////////////////////////////////////////////////////////////////

class ImageDetails
{
public:
    static ImageDetails  MakeFromCopy (const DiskCopy & copy);

    std::string      fileName;
    DiskFormat       format       = DiskFormat::Dsk;
    uint64_t         fileSize     = 0;
    bool             isReadOnly   = false;
    bool             isWoz        = false;
    bool             isCrcMatch   = true;
    WozInfo          info;
    WozFileLayout    layout;
    vector<Finding>  problems;

private:
    static void  CheckLargestTrack (const WozFileLayout & layout, const WozInfo & info, vector<Finding> & inOut);
    static void  CheckMeta         (const WozFileLayout & layout, vector<Finding> & inOut);
    static void  CheckChunks       (const WozFileLayout & layout, vector<Finding> & inOut);
    static void  CheckRecords      (const WozFileLayout & layout, vector<Finding> & inOut);
    static bool  IsRfc3339         (std::string_view text);
    static bool  IsInList          (std::string_view value, std::span<const std::string_view> list);
    static void  AddProblem        (vector<Finding> & inOut, FindingKind kind, int value, int value2, const std::string & detail);
};
