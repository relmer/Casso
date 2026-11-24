#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskCommandResult.h"

class IDiskFileIo;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskStampCommand
//
//  The disk tool's `stamp`: a WOZ image's format stamp as a table, or with
//  --json as the JSON the file stores. Exits with 0 for an image with a
//  stamp, kNoStamp for a WOZ without one, kUnreadable for a stamp this build
//  can't read, and 1 for a file that is not a WOZ or can't be read.
//
////////////////////////////////////////////////////////////////////////////////

class DiskStampCommand
{
public:
    static constexpr int  kNoStamp    = DiskCommandResult::kNoOutput;
    static constexpr int  kUnreadable = 3;

    static void  Run (IDiskFileIo & io, const std::string & imagePath, bool isJson, DiskCommandResult & result);
};
