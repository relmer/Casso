#pragma once

#include "Pch.h"

class IDiskFileIo;
class IDriveSpace;





//  The volume a repaired copy is made of.
struct BlockRepairSource
{
    bool          isCard         = false;
    std::wstring  imagePath;
    std::wstring  hostFileName;
    std::wstring  volumeLabel;
    bool          isSectorVolume = false;
    uint64_t      volumeBytes    = 0;
};


//  Where the copy goes: `host` is the file written, the copy or, for an
//  image in a container, the copy of its container, and `rest` is the
//  image's path inside it. With `isUserChoice`, the user chooses `host`,
//  after `why` when it isn't empty.
struct BlockRepairDestination
{
    std::filesystem::path  host;
    std::wstring           rest;
    uint64_t               bytes        = 0;
    std::wstring           why;
    bool                   isUserChoice = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  BlockRepairTarget
//
//  The repair confirmation's introduction, and where the repaired copy of a
//  volume goes.
//
////////////////////////////////////////////////////////////////////////////////

class BlockRepairTarget
{
public:
    static constexpr int  kRepairIntroLines = 3;
    static constexpr int  kCautionLines     = 3;

    static std::wstring            BuildIntro        (bool isCard, const std::wstring & fileName, const std::wstring & hostFileName, bool isSuspect, bool isDos, int & outLines);
    static BlockRepairDestination  ChooseDestination (const BlockRepairSource & source, IDiskFileIo & fileIo, const IDriveSpace & drives);

private:
    static std::wstring  FormatCardIntro (const std::wstring & fileName);
    static std::wstring  FormatCaution   (bool isDos);
};
