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
    std::string         path;
    Section             section = Section::None;
    int                 drive   = 0;



    while (std::getline (in, line))
    {
        if (!line.empty() && line.back() == '\r')
        {
            line.pop_back();
            config.hasCarriageReturns = true;
        }

        config.m_lines.push_back (line);
        section = GetSection (line, section);

        if (section == Section::Settings && HasKey (line, kBootDelay))
        {
            int  value = line[kBootDelay.size()] - '0';

            config.bootDelay = (value >= 0 && value <= kBootDelayMax) ? value : config.bootDelay;
        }

        if (section == Section::Drives && HasKey (line, kNumber))
        {
            int  value = line[kNumber.size()] - '0';

            config.number = (value >= 2 && value <= kDrivesMax && value % 2 == 0) ? value : config.number;
        }

        if (section == Section::Drives && TryReadDrive (line, drive, path))
        {
            config.paths[drive] = path;
        }
    }

    return config;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::GetSection
//
//  As the firmware reads it: a line starting with [ starts a section, and
//  one other than [settings] or [drives] is ignored to its end.
//
////////////////////////////////////////////////////////////////////////////////

A2retroNetConfig::Section A2retroNetConfig::GetSection (const std::string & line, Section section)
{
    if (line.empty() || line[0] != '[')
    {
        return section;
    }

    if (_strnicmp (line.c_str(), kSettings.data(), kSettings.size()) == 0)
    {
        return Section::Settings;
    }

    return _strnicmp (line.c_str(), kDrives.data(), kDrives.size()) == 0 ? Section::Drives : Section::Other;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::HasKey
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::HasKey (const std::string & line, std::string_view key)
{
    return line.size() > key.size() && _strnicmp (line.c_str(), key.data(), key.size()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::TryReadDrive
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::TryReadDrive (std::string_view line, int & outDrive, std::string & outPath)
{
    if (line.size() < 2 || line[0] < '1' || line[0] > '0' + kDrivesMax || line[1] != '=')
    {
        return false;
    }

    outDrive = line[0] - '1';
    outPath  = std::string (line.substr (2));

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::Write
//
//  Of several lines for one drive, the firmware takes the last, so that one
//  is set. A missing number= goes just after the [drives] header, and a
//  missing drive line at the end of [drives]. Additions are listed in their
//  order in the text, then inserted from the last position back, so no
//  insertion moves a position found before it.
//
////////////////////////////////////////////////////////////////////////////////

std::string A2retroNetConfig::Write() const
{
    std::vector<std::string>  lines        = m_lines;
    std::vector<Addition>     additions;
    Section                   section      = Section::None;
    size_t                    settingsAt   = SIZE_MAX;
    size_t                    drivesAt     = SIZE_MAX;
    size_t                    drivesEnd    = SIZE_MAX;
    size_t                    driveLine[kDrivesMax];
    bool                      hasBootDelay = false;
    bool                      hasNumber    = false;
    bool                      addsDrives   = number != kDrivesMax;
    int                       drive        = 0;
    std::string               path;
    std::string               text;



    if (m_lines.empty())
    {
        return WriteCanonical();
    }

    std::fill (std::begin (driveLine), std::end (driveLine), SIZE_MAX);

    for (size_t i = 0; i < lines.size(); i++)
    {
        section = GetSection (lines[i], section);

        settingsAt = (section == Section::Settings && settingsAt == SIZE_MAX) ? i : settingsAt;
        drivesAt   = (section == Section::Drives   && drivesAt   == SIZE_MAX) ? i : drivesAt;
        drivesEnd  = section == Section::Drives ? i + 1 : drivesEnd;

        if (section == Section::Settings && HasKey (lines[i], kBootDelay))
        {
            lines[i]     = std::format ("{}{}", kBootDelay, bootDelay);
            hasBootDelay = true;
        }

        if (section == Section::Drives && HasKey (lines[i], kNumber))
        {
            lines[i]  = std::format ("{}{}", kNumber, number);
            hasNumber = true;
        }

        if (section == Section::Drives && TryReadDrive (lines[i], drive, path))
        {
            driveLine[drive] = i;
        }
    }

    if (!hasBootDelay && bootDelay != kBootDelayDefault)
    {
        if (settingsAt == SIZE_MAX)
        {
            additions.push_back (Addition { 0, std::string (kSettings) });
        }

        additions.push_back (Addition { settingsAt == SIZE_MAX ? 0 : settingsAt + 1, std::format ("{}{}", kBootDelay, bootDelay) });
    }

    for (drive = 0; drive < kDrivesMax; drive++)
    {
        addsDrives = addsDrives || (driveLine[drive] == SIZE_MAX && !paths[drive].empty());
    }

    //  With no [drives] in the text, its lines go after a new header at the
    //  end.
    if (drivesAt == SIZE_MAX && addsDrives)
    {
        additions.push_back (Addition { lines.size(), std::string (kDrives) });
    }

    if (!hasNumber && number != kDrivesMax)
    {
        additions.push_back (Addition { drivesAt == SIZE_MAX ? lines.size() : drivesAt + 1, std::format ("{}{}", kNumber, number) });
    }

    for (drive = 0; drive < kDrivesMax; drive++)
    {
        if (driveLine[drive] != SIZE_MAX)
        {
            lines[driveLine[drive]] = std::format ("{}={}", drive + 1, paths[drive]);
        }
        else if (!paths[drive].empty())
        {
            additions.push_back (Addition { drivesEnd == SIZE_MAX ? lines.size() : drivesEnd, std::format ("{}={}", drive + 1, paths[drive]) });
        }
    }

    std::stable_sort (additions.begin(), additions.end(), [] (const Addition & a, const Addition & b) { return a.at < b.at; });

    //  Of additions at one position, the last listed is inserted first, so
    //  they end up in the order listed.
    for (auto it = additions.rbegin(); it != additions.rend(); ++it)
    {
        lines.insert (lines.begin() + (ptrdiff_t) it->at, it->line);
    }

    for (const std::string & line : lines)
    {
        text += line + "\n";
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::GetAlternatives
//
////////////////////////////////////////////////////////////////////////////////

std::vector<std::string> A2retroNetConfig::GetAlternatives (int drive) const
{
    std::vector<std::string>  alternatives;
    Section                   section = Section::None;
    int                       found   = 0;
    std::string               path;



    for (const std::string & line : m_lines)
    {
        section = GetSection (line, section);

        if (section == Section::Drives && !line.empty() && line[0] == kComment && TryReadDrive (std::string_view (line).substr (1), found, path) && found == drive)
        {
            alternatives.push_back (path);
        }
    }

    return alternatives;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::SwitchToAlternative
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::SwitchToAlternative (int drive, const std::string & path)
{
    Section      section  = Section::None;
    int          found    = 0;
    std::string  existing;



    for (size_t i = 0; i < m_lines.size(); i++)
    {
        section = GetSection (m_lines[i], section);

        if (section != Section::Drives || m_lines[i].empty() || m_lines[i][0] != kComment)
        {
            continue;
        }

        if (TryReadDrive (std::string_view (m_lines[i]).substr (1), found, existing) && found == drive && existing == path)
        {
            if (paths[drive].empty())
            {
                m_lines.erase (m_lines.begin() + (ptrdiff_t) i);
            }
            else
            {
                m_lines[i] = std::format ("{}{}={}", kComment, drive + 1, paths[drive]);
            }

            paths[drive] = path;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::WriteCanonical
//
//  A line for each drive in use, empty for a drive with no image, as the
//  firmware's configuration utility writes the file.
//
////////////////////////////////////////////////////////////////////////////////

std::string A2retroNetConfig::WriteCanonical() const
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
    return CheckImage (fileName, bytes.first (std::min (bytes.size(), kTwoImgHeader)), bytes.size(), outProblem);
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::CheckImage
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::CheckImage (const std::string & fileName, std::span<const Byte> header, uint64_t size, std::string & outProblem)
{
    std::string  extension = std::filesystem::path (fileName).extension().string();
    bool         isTwoImg  = false;
    size_t       offset    = 0;
    uint64_t     blocks    = 0;



    outProblem.clear();

    std::transform (extension.begin(), extension.end(), extension.begin(), [] (char c) { return (char) std::tolower ((unsigned char) c); });

    isTwoImg = extension == ".2mg";
    offset   = isTwoImg ? kTwoImgHeader : 0;

    if (!IsImageName (fileName))
    {
        outProblem = "A2retroNET reads only .po, .hdv and .2mg images.";
    }
    else if (size < offset + kBlockBytes || (isTwoImg && header.size() < kTwoImgHeader))
    {
        outProblem = "The image is too short to have a block.";
    }
    else if (isTwoImg && (header[kTwoImgOffsetAt] | (header[kTwoImgOffsetAt + 1] << 8)) != (int) kTwoImgHeader)
    {
        outProblem = "A2retroNET reads a .2mg's blocks from just after its 64-byte header, and this image's start elsewhere.";
    }
    else if (isTwoImg && header[kTwoImgFormatAt] != kTwoImgProDos)
    {
        outProblem = "A2retroNET reads a .2mg's blocks in ProDOS order, and this image is in another order.";
    }

    blocks = (size - std::min (size, (uint64_t) offset)) / kBlockBytes;

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





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::SplitDevice
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::SplitDevice (const std::string & path, std::string & outPath)
{
    bool  hasDevice = false;



    outPath = path;

    for (std::string_view device : { kSd, kUsb })
    {
        if (!hasDevice && outPath.size() >= device.size() && _strnicmp (outPath.c_str(), device.data(), device.size()) == 0)
        {
            outPath   = outPath.substr (device.size());
            hasDevice = true;
        }
    }

    if (!outPath.empty() && outPath[0] == '/')
    {
        outPath = outPath.substr (1);
    }

    return hasDevice;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::SwapDrives
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetConfig::SwapDrives (int first, int second)
{
    Section      section = Section::None;
    int          found   = 0;
    std::string  path;



    std::swap (paths[first], paths[second]);

    for (std::string & line : m_lines)
    {
        section = GetSection (line, section);

        if (section == Section::Drives && !line.empty() && line[0] == kComment && TryReadDrive (std::string_view (line).substr (1), found, path))
        {
            found = found == first ? second : found == second ? first : found;
            line  = std::format ("{}{}={}", kComment, found + 1, path);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::IsImageName
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::IsImageName (const std::string & fileName)
{
    std::string  extension = std::filesystem::path (fileName).extension().string();



    std::transform (extension.begin(), extension.end(), extension.begin(), [] (char c) { return (char) std::tolower ((unsigned char) c); });

    return extension == ".po" || extension == ".hdv" || extension == ".2mg";
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::IsSamePath
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::IsSamePath (const std::string & drivePath, const std::string & path)
{
    std::string  driveRelative;
    std::string  relative;



    SplitDevice (drivePath, driveRelative);
    SplitDevice (path, relative);

    return !driveRelative.empty() && _stricmp (driveRelative.c_str(), relative.c_str()) == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::FindDrives
//
////////////////////////////////////////////////////////////////////////////////

std::vector<int> A2retroNetConfig::FindDrives (const std::string & path) const
{
    std::vector<int>  drives;



    for (int drive = 0; drive < kDrivesMax; drive++)
    {
        if (IsSamePath (paths[drive], path))
        {
            drives.push_back (drive);
        }
    }

    return drives;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::MovePath
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::MovePath (const std::string & from, const std::string & to)
{
    Section      section = Section::None;
    int          drive   = 0;
    std::string  path;
    std::string  moved;
    bool         isMoved = false;



    for (std::string & each : paths)
    {
        if (TryMovePath (each, from, to, moved))
        {
            each    = moved;
            isMoved = true;
        }
    }

    for (std::string & line : m_lines)
    {
        section = GetSection (line, section);

        if (section == Section::Drives && !line.empty() && line[0] == kComment && TryReadDrive (std::string_view (line).substr (1), drive, path) && TryMovePath (path, from, to, moved))
        {
            line    = std::format ("{}{}={}", kComment, drive + 1, moved);
            isMoved = true;
        }
    }

    return isMoved;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::TryMovePath
//
//  The drive's path is the file moved, or is in the folder moved, whose
//  path the new one replaces.
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::TryMovePath (const std::string & drivePath, const std::string & from, const std::string & to, std::string & outPath)
{
    std::string  relative;
    std::string  folder = from + "/";



    SplitDevice (drivePath, relative);

    if (IsSamePath (relative, from))
    {
        outPath = to;
        return true;
    }

    if (relative.size() > folder.size() && _strnicmp (relative.c_str(), folder.c_str(), folder.size()) == 0)
    {
        outPath = to + "/" + relative.substr (folder.size());
        return true;
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::ClearPath
//
////////////////////////////////////////////////////////////////////////////////

bool A2retroNetConfig::ClearPath (const std::string & path)
{
    std::vector<int>  drives = FindDrives (path);



    for (int drive : drives)
    {
        paths[drive].clear();
    }

    return !drives.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::MoveDrive
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetConfig::MoveDrive (int from, int to)
{
    int  step = to > from ? 1 : -1;



    for (int drive = from; drive != to; drive += step)
    {
        SwapDrives (drive, drive + step);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfig::SetDrive
//
////////////////////////////////////////////////////////////////////////////////

void A2retroNetConfig::SetDrive (int drive, const std::string & path)
{
    std::vector<std::string>  alternatives = GetAlternatives (drive);
    bool                      switched     = false;



    for (int other = 0; !path.empty() && other < kDrivesMax; other++)
    {
        if (other != drive && IsSamePath (paths[other], path))
        {
            paths[other].clear();
        }
    }

    if (std::find (alternatives.begin(), alternatives.end(), path) != alternatives.end())
    {
        switched = SwitchToAlternative (drive, path);
    }

    if (!switched)
    {
        paths[drive] = path;
    }
}
