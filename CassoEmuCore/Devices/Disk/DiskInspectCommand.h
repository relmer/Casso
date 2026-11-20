#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskCommandResult.h"

class IDiskFileIo;
class BlockInspectorModel;
struct BlockMap;
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
//  An image block mode opens, such as an 800K disk or a hard disk, is
//  analyzed as block mode does. The summary is the image's block count and
//  each volume's file system, name and verdict; each finding is the volume,
//  its blocks, or a DOS volume's tracks and sectors, and the finding's text.
//
////////////////////////////////////////////////////////////////////////////////

class DiskInspectCommand
{
public:
    //  A block volume's finding and the map units it covers. A finding of
    //  damaged blocks is a role, as "Marked used, in no file".
    struct BlockFinding
    {
        std::wstring      text;
        vector<uint32_t>  units;
        bool              isRole = false;
    };

    static void         Run           (IDiskFileIo & io, const std::string & imagePath, DiskCommandResult & result);
    static std::string  FormatSummary (const DiskAnalysis & analysis);
    static std::string  FormatFinding (const Finding & finding);

    //  The findings block mode lists for a volume, then a file whose blocks
    //  can't be followed and a directory that can't be read.
    static vector<BlockFinding>  GetBlockFindings (const BlockMap & map);

    //  "volume 1, HFS "Casso Walk"", and the units as the image's blocks, or
    //  as a DOS volume's tracks and sectors, in runs: "3-5,9" or "17/0-3".
    static std::string           FormatVolume     (const BlockInspectorModel & model, int volume);
    static std::string           FormatUnits      (const BlockInspectorModel & model, int volume, const vector<uint32_t> & units);

private:
    static void  RunBlocks (IDiskFileIo & io, const std::string & imagePath, DiskCommandResult & result);
};
