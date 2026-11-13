#include "Pch.h"

#include "Devices/Disk/A2retroNetConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::Parse
//
//  As the firmware reads it: a section line sets the section, and a line
//  the section doesn't take is ignored. A setting out of range is ignored.
//  A CR before a line's \n is removed, unlike the firmware, so the file is
//  written again without it.
//
////////////////////////////////////////////////////////////////////////////////

A2retroNetConfig A2retroNetConfig::Parse (const std::string & text)
{
    A2retroNetConfig    config;
    std::istringstream  in (text);
    std::string         line;
    bool                inSettings = false;
    bool                inDrives   = false;



    while (std::getline (in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.empty() || line[0] == '#')
        {
            continue;
        }

        if (line[0] == '[')
        {
            inSettings = _strnicmp (line.c_str(), kSettings.data(), kSettings.size()) == 0;
            inDrives   = _strnicmp (line.c_str(), kDrives.data(),   kDrives.size())   == 0;
            continue;
        }

        if (inSettings && line.size() > kBootDelay.size() && _strnicmp (line.c_str(), kBootDelay.data(), kBootDelay.size()) == 0)
        {
            int  value = line[kBootDelay.size()] - '0';

            config.bootDelay = (value >= 0 && value <= kBootDelayMax) ? value : config.bootDelay;
        }

        if (inDrives && line.size() > kNumber.size() && _strnicmp (line.c_str(), kNumber.data(), kNumber.size()) == 0)
        {
            int  value = line[kNumber.size()] - '0';

            config.number = (value >= 2 && value <= kDrivesMax && value % 2 == 0) ? value : config.number;
        }

        if (inDrives && line.size() >= 2 && line[0] >= '1' && line[0] <= '0' + kDrivesMax && line[1] == '=')
        {
            config.paths[line[0] - '1'] = line.substr (2);
        }
    }

    return config;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::Write
//
//  A line for each drive in use, empty for a drive with no image, as the
//  firmware's configuration utility writes the file.
//
////////////////////////////////////////////////////////////////////////////////

std::string A2retroNetConfig::Write() const
{
    std::string  text = std::format ("[settings]\nbootdelay={}\n[drives]\nnumber={}\n", bootDelay, number);



    for (int drive = 0; drive < number; drive++)
    {
        text += std::format ("{}={}\n", drive + 1, paths[drive]);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::TryFindFreeDrive
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::TryFindFreeDrive (int & outDrive) const
{
    bool  found = false;



    for (int drive = 0; !found && drive < number; drive++)
    {
        found    = paths[drive].empty();
        outDrive = found ? drive : outDrive;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::CheckImage
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::CheckImage (const std::string & fileName, std::span<const Byte> bytes, std::string & outProblem)
{
    std::string  extension = std::filesystem::path (fileName).extension().string();
    bool         isTwoImg  = false;
    size_t       offset    = 0;
    size_t       blocks    = 0;



    outProblem.clear();

    std::transform (extension.begin(), extension.end(), extension.begin(), [] (char c) { return (char) std::tolower ((unsigned char) c); });

    isTwoImg = extension == ".2mg";
    offset   = isTwoImg ? kTwoImgHeader : 0;

    if (extension != ".po" && extension != ".hdv" && !isTwoImg)
    {
        outProblem = "A2retroNET reads only .po, .hdv and .2mg images.";
    }
    else if (bytes.size() < offset + kBlockBytes)
    {
        outProblem = "The image is too short to have a block.";
    }
    else if (isTwoImg && (bytes[kTwoImgOffsetAt] | (bytes[kTwoImgOffsetAt + 1] << 8)) != (int) kTwoImgHeader)
    {
        outProblem = "A2retroNET reads a .2mg's blocks from just after its 64-byte header, and this image's start elsewhere.";
    }
    else if (isTwoImg && bytes[kTwoImgFormatAt] != kTwoImgProDos)
    {
        outProblem = "A2retroNET reads a .2mg's blocks in ProDOS order, and this image is in another order.";
    }

    blocks = (bytes.size() - std::min (bytes.size(), offset)) / kBlockBytes;

    if (outProblem.empty() && blocks > kBlocksMax)
    {
        outProblem = std::format ("A2retroNET reads at most {} blocks, and this image has {}.", kBlocksMax, blocks);
    }

    return outProblem.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::MakePath
//
////////////////////////////////////////////////////////////////////////////////

std::string A2retroNetConfig::MakePath (const std::filesystem::path & root, const std::filesystem::path & image)
{
    std::string  path = image.lexically_relative (root).generic_string();



    return path.empty() ? image.filename().generic_string() : path;
}
