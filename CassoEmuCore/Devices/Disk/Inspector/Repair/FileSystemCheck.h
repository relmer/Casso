#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FileSystemCheck
//
//  The file-system verdict of a disk image as text, its first finding of a
//  weight, and the analysis of an image's bytes with no window, as Full Send
//  and the command line run it.
//
////////////////////////////////////////////////////////////////////////////////

class FileSystemCheck
{
public:
    //  OK, Suspect, Bad, or No standard file system.
    static std::wstring  FormatVerdict (FileSystemVerdict verdict);

    //  The first file-system finding of the weight, in the analysis's order,
    //  or nullptr.
    static const Finding *  GetFirstFinding (const DiskAnalysis & analysis, FindingWeight weight);

    //  The finding for the verdict: the first of damage for Bad,
    //  the first worth a look for Suspect, nullptr otherwise.
    static const Finding *  GetVerdictFinding (const DiskAnalysis & analysis);

    //  An image file's bytes analyzed as a drive mounts them; a file that
    //  can't be loaded fails with the reason a mount fails with.
    static HRESULT  Analyze (const vector<Byte> & bytes, const std::string & utf8Path, DiskAnalysis & outAnalysis, std::wstring & outReason);
};
