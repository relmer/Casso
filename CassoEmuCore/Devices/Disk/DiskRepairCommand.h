#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskCommandResult.h"

class IDiskFileIo;
struct CommandLineOptions;
struct DiskAnalysis;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepairCommand
//
//  The disk tool's `repair`: the image's file-system verdict, its findings,
//  and the changes Repair makes, a line each, then the repaired copy written
//  as the inspector writes it. With --list nothing is written. A volume that
//  can't be repaired exits with complaints and the reason. An image block
//  mode opens is checked volume by volume as block mode checks it, and every
//  volume's changes go into one copy.
//
////////////////////////////////////////////////////////////////////////////////

class DiskRepairCommand
{
public:
    static void         Run           (IDiskFileIo & io, const CommandLineOptions & options, DiskCommandResult & result);

    //  The verdict line `test` and `repair` both write, and the image's
    //  file-system findings, a line each.
    static std::string  FormatVerdict (const std::string & imagePath, const DiskAnalysis & analysis);

    //  The verdict line `test` adds for a 5.25-inch image, and its finding for
    //  Bad or Suspect; Bad is a complaint. An image the inspector doesn't
    //  read, such as an 800K one, adds nothing.
    static void         AddVerdict    (IDiskFileIo & io, const std::string & imagePath, DiskCommandResult & result);

private:
    static void  RunBlocks (IDiskFileIo & io, const CommandLineOptions & options, DiskCommandResult & result);
};
