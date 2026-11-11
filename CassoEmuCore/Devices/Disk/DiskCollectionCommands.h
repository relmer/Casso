#pragma once

#include "Pch.h"

#include "CommandLineOptions.h"
#include "Devices/Disk/DiskCommandResult.h"

class IDiskFileIo;
struct CollectionDisk;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCollectionCommands
//
//  The disk tool's commands over a folder of images: `list` of a folder
//  writes Disk list's CSV, `dupes` writes Find duplicates' CSV, and `search`
//  writes the path of each file inside an image whose text has the words,
//  one a line. Each walks the folder as the window's tools do.
//
////////////////////////////////////////////////////////////////////////////////

class DiskCollectionCommands
{
public:
    //  `dupes` or `search`, whose first operand is a folder.
    static void  Run       (IDiskFileIo & io, const CommandLineOptions & options, DiskCommandResult & result);

    static void  RunList   (IDiskFileIo & io, const std::string & folder, DiskCommandResult & result);
    static void  RunDupes  (IDiskFileIo & io, const std::string & folder, DiskCommandResult & result);
    static void  RunSearch (IDiskFileIo & io, const std::string & folder, const std::string & text, DiskCommandResult & result);

private:
    static HRESULT      Collect          (IDiskFileIo & io, const std::string & folder, std::vector<CollectionDisk> & outDisks, DiskCommandResult & result);
    static void         ReportUnreadable (const std::string & folder, DiskCommandResult & result);
    static std::string  GetPrintable     (const std::string & path);
};
