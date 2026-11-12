#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskCommandResult.h"

class IDiskFileIo;
struct DiskAnalysis;
struct Finding;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskInspectCommand
//
//  The disk tool's `inspect`: the disk inspector's analysis of an image, as
//  its status bar and Issues list have it. The first line is the summary;
//  each line after it is a finding, tab-separated: the quarter track as a
//  track number, the category, the sector, the cell and the finding's text.
//  A field the finding doesn't have is a hyphen.
//
////////////////////////////////////////////////////////////////////////////////

class DiskInspectCommand
{
public:
    static void         Run           (IDiskFileIo & io, const std::string & imagePath, DiskCommandResult & result);
    static std::string  FormatSummary (const DiskAnalysis & analysis);
    static std::string  FormatFinding (const Finding & finding);
};
