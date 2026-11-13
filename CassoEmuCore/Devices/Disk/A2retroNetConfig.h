#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig
//
//  A2retroNET.txt, the file in the root of an A2Pico's micro SD card or USB
//  drive that sets which disk image each of the A2retroNET firmware's
//  SmartPort drives has, as its README and its config.c read and write it.
//
//  The file is two INI sections, [settings] with bootdelay=0 to 9 and
//  [drives] with number=2, 4, 6 or 8 and a line n=path for each drive 1 to
//  8, the path from the root with no spaces around the =. A line starting
//  with # is a comment. The firmware removes only the \n that ends a line,
//  so the file is written with LF line ends; a CR before the \n would be
//  part of the path.
//
//  The firmware reads a ProDOS block image of up to 65,535 blocks: a .po or
//  .hdv file of blocks, or a .2mg whose blocks start after its 64-byte
//  header. A .2mg's header fields aren't read, so its data must start at 64
//  and be in ProDOS order. The block count is the file's bytes after the
//  header divided by 512.
//
////////////////////////////////////////////////////////////////////////////////

class A2retroNetConfig
{
public:
    static constexpr int          kDrivesMax        = 8;
    static constexpr int          kBootDelayDefault = 3;
    static constexpr int          kBootDelayMax     = 9;
    static constexpr size_t       kBlockBytes       = 512;
    static constexpr size_t       kBlocksMax        = 65535;
    static constexpr size_t       kTwoImgHeader     = 64;
    static constexpr const char * kpszFileName      = "A2retroNET.txt";

    int          bootDelay = kBootDelayDefault;
    int          number    = kDrivesMax;
    std::string  paths[kDrivesMax];

    //  The settings in the file's text, the firmware's defaults for what it
    //  doesn't set.
    static A2retroNetConfig  Parse (const std::string & text);

    //  The text the firmware writes, with LF line ends.
    std::string  Write () const;

    //  The first drive, from 0, among the number in use with no image, or
    //  false when each has one.
    bool  TryFindFreeDrive (int & outDrive) const;

    //  Whether the firmware reads the image as a drive, and when it doesn't,
    //  why, in a sentence.
    static bool  CheckImage (const std::string & fileName, std::span<const Byte> bytes, std::string & outProblem);

    //  A path for the file: from the card's root, with / between folders.
    static std::string  MakePath (const std::filesystem::path & root, const std::filesystem::path & image);

private:
    static constexpr std::string_view  kSettings  = "[settings]";
    static constexpr std::string_view  kDrives    = "[drives]";
    static constexpr std::string_view  kBootDelay = "bootdelay=";
    static constexpr std::string_view  kNumber    = "number=";

    static constexpr size_t  kTwoImgOffsetAt = 0x18;
    static constexpr size_t  kTwoImgFormatAt = 0x0C;
    static constexpr Byte    kTwoImgProDos   = 1;
};
