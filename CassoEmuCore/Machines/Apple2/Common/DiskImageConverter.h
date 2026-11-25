#pragma once

#include "Pch.h"

class SectorDecodeReport;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageFormat
//
//  The image formats a disk image can be converted to.
//
////////////////////////////////////////////////////////////////////////////////

enum class DiskImageFormat
{
    Dsk,
    Po,
    TwoImg,
    Hdv,
    Woz,
    Nib,
    Nb2,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskImageConverter
//
//  A disk image's bytes rewritten in another image format. A sector image
//  becomes another sector order, a 2IMG, a WOZ, a .nib or a .nb2; a .nib,
//  .nb2 or .woz becomes another format only when every track reads as 16
//  whole sectors. Pure: no I/O, no Win32.
//
////////////////////////////////////////////////////////////////////////////////

class DiskImageConverter
{
public:
    //  The formats offered for an image, by its extension and file size.
    //  Empty for a file that can't be converted.
    static void           GetTargets   (const std::string & path, uint64_t fileBytes, std::vector<DiskImageFormat> & outTargets);

    //  The extension a format is written with, such as L".dsk".
    static const wchar_t * GetExtension (DiskImageFormat format);

    //  The image's bytes in the target format. On failure, outReason is a
    //  sentence with the reason.
    static HRESULT        Convert      (const std::vector<Byte> & fileBytes, const std::string & path, DiskImageFormat target, std::vector<Byte> & outBytes, std::string & outReason);

private:
    static std::string  GetLowerExtension  (const std::string & path);
    static bool         IsBitStream        (const std::string & extension);
    static HRESULT      CheckEveryTrack    (const SectorDecodeReport & report, std::string & outReason);
    static HRESULT      BuildWoz           (const std::vector<Byte> & sectors, std::vector<Byte> & outBytes, std::string & outReason);
    static HRESULT      BuildNibbles       (const std::vector<Byte> & sectors, size_t trackSize, std::vector<Byte> & outBytes, std::string & outReason);
    static void         BuildTwoImg        (const std::vector<Byte> & sectors, std::vector<Byte> & outBytes);
};
