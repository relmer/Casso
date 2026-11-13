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
//  8, the path from the root with no spaces around the =. A path starting
//  with sd: or usb: is on that device. A line starting with # is a comment,
//  and the README suggests commenting out a drive's other images to switch
//  between them, so a change rewrites only the lines it sets. The firmware's
//  configuration utility writes the file again without comments. The
//  firmware removes only the \n that ends a line, so the file is written
//  with LF line ends; a CR before the \n would be part of the path.
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
    static constexpr int               kDrivesMax        = 8;
    static constexpr int               kBootDelayDefault = 3;
    static constexpr int               kBootDelayMax     = 9;
    static constexpr size_t            kBlockBytes       = 512;
    static constexpr size_t            kBlocksMax        = 65535;
    static constexpr size_t            kTwoImgHeader     = 64;
    static constexpr const char      * kpszFileName      = "A2retroNET.txt";
    static constexpr const wchar_t   * kpszWideFileName  = L"A2retroNET.txt";

    int          bootDelay          = kBootDelayDefault;
    int          number             = kDrivesMax;
    std::string  paths[kDrivesMax];

    //  Whether the parsed text has CR LF line ends, with which each path
    //  ends in a CR and the firmware doesn't open any image.
    bool         hasCarriageReturns = false;

    //  The settings in the file's text, the firmware's defaults for what it
    //  doesn't set.
    static A2retroNetConfig  Parse (const std::string & text);

    //  The parsed text with LF line ends, and each line for a setting or a
    //  drive's image set to its value. A value missing from the text is
    //  added as a line in its section, unless the value is the firmware's
    //  default. With no parsed text, the text the firmware writes.
    std::string  Write () const;

    //  The first drive, from 0, among the number in use with no image, or
    //  false when each has one.
    bool  TryFindFreeDrive (int & outDrive) const;

    //  The images commented out for a drive, from 0, in the file's order.
    std::vector<std::string>  GetAlternatives (int drive) const;

    //  Sets the drive's image to one of its commented-out images. Its
    //  current image, if any, is commented out in that line's place.
    bool  SwitchToAlternative (int drive, const std::string & path);

    //  Swaps two drives' images, from 0, and the images commented out for
    //  each.
    void  SwapDrives (int first, int second);

    //  Whether the file's extension is .po, .hdv or .2mg, in any case.
    static bool  IsImageName (const std::string & fileName);

    //  Whether the firmware reads the image as a drive, and when it doesn't,
    //  why, in a sentence.
    static bool  CheckImage (const std::string & fileName, std::span<const Byte> bytes, std::string & outProblem);

    //  The same check from the image's first kTwoImgHeader bytes, or fewer
    //  in a shorter file, and its size.
    static bool  CheckImage (const std::string & fileName, std::span<const Byte> header, uint64_t size, std::string & outProblem);

    //  The path from the root, without an sd: or usb: device or a leading /.
    //  True when the path has a device, which may be the other one.
    static bool  SplitDevice (const std::string & path, std::string & outPath);

    //  A path for the file: from the card's root, with / between folders.
    static std::string  MakePath (const std::filesystem::path & root, const std::filesystem::path & image);

private:
    enum class Section { None, Settings, Drives, Other };

    struct Addition
    {
        size_t       at = 0;
        std::string  line;
    };

    static constexpr std::string_view  kSettings  = "[settings]";
    static constexpr std::string_view  kDrives    = "[drives]";
    static constexpr std::string_view  kBootDelay = "bootdelay=";
    static constexpr std::string_view  kNumber    = "number=";
    static constexpr char              kComment   = '#';
    static constexpr std::string_view  kSd        = "sd:";
    static constexpr std::string_view  kUsb       = "usb:";

    static constexpr size_t  kTwoImgOffsetAt = 0x18;
    static constexpr size_t  kTwoImgFormatAt = 0x0C;
    static constexpr Byte    kTwoImgProDos   = 1;

    //  The section a line starts, or `section` for any other line.
    static Section  GetSection      (const std::string & line, Section section);
    static bool     HasKey          (const std::string & line, std::string_view key);

    //  Whether the line is n=path, with the drive from 0.
    static bool     TryReadDrive    (std::string_view line, int & outDrive, std::string & outPath);

    std::string     WriteCanonical  () const;

    std::vector<std::string>  m_lines;
};
