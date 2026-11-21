#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "Devices/Disk/DiskCommandResult.h"
#include "Devices/Disk/IDiskCommandContext.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSectorCommands
//
//  The disk tool's sectorread, sectorwrite, blockread and blockwrite
//  commands, which read and write an image's sectors and blocks with no
//  file system.
//
////////////////////////////////////////////////////////////////////////////////

class DiskSectorCommands
{
public:
    explicit DiskSectorCommands (IDiskCommandContext & context) : m_context (context) {}

    //
    //  Lays a file from the host into an image at a track and a DOS logical sector,
    //  with no filesystem involved.
    //
    //  FOR THE DISKS THAT HAVE NO FILESYSTEM TO PUT A FILE INTO. A demo that
    //  boots its own loader and reads fixed tracks is a real and common
    //  layout, and `put` cannot express it: there is no catalog to make an
    //  entry in and no allocator to ask for space. This writes exactly the
    //  bytes given, exactly where it is told.
    //
    void  RunSectorRead  (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunSectorWrite (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunBlockRead   (const CommandLineOptions & options, DiskCommandResult & result);
    void  RunBlockWrite  (const CommandLineOptions & options, DiskCommandResult & result);

    //  The record a sector command's Nth position addresses, as a byte
    //  offset into the DOS-ordered buffer. Positions advance linearly --
    //  sector, then track -- and each maps through the stated numbering
    //  independently.
    static size_t  GetSectorRecordOffset (CommandLineOptions::DiskOptions::Numbering numbering,
                                          size_t                                     running);

private:
    IDiskCommandContext  & m_context;
};
