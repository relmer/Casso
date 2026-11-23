#include "Pch.h"

#include "FileTypes/SaveSummary.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "FileTypes/WizardryScenario.h"
#include "Machines/Apple2/Common/InfocomSave.h"
#include "Machines/Apple2/Common/IVolume.h"
#include "Machines/Apple2/Common/VolumeImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  SaveSummary::Find
//
////////////////////////////////////////////////////////////////////////////////

bool SaveSummary::Find (const std::vector<Byte> & sectors, std::vector<std::string> & outRows)
{
    std::vector<std::string>  lines;
    bool                      found = false;



    outRows.clear();

    if (FindInfocom (sectors, lines))
    {
        MakeRows ("Infocom saved games", lines, outRows);
        found = true;
    }
    else if (FindWizardry (sectors, lines))
    {
        MakeRows ("Wizardry saved game", lines, outRows);
        found = true;
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveSummary::MakeRows
//
////////////////////////////////////////////////////////////////////////////////

void SaveSummary::MakeRows (const std::string & heading, const std::vector<std::string> & lines, std::vector<std::string> & outRows)
{
    outRows.clear();

    Wrap (heading, outRows);
    outRows.push_back (std::string());

    for (const std::string & line : lines)
    {
        Wrap (line, outRows);
    }

    if (outRows.size() > kRows)
    {
        outRows.resize (kRows);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveSummary::FindInfocom
//
//  A disk with no file system Casso recognizes, whose positions have a
//  story's dynamic memory, each saved position a line.
//
////////////////////////////////////////////////////////////////////////////////

bool SaveSummary::FindInfocom (const std::vector<Byte> & sectors, std::vector<std::string> & outLines)
{
    std::vector<InfocomSave::Position>  positions;



    outLines.clear();

    if (VolumeImage::DetectFilesystem (sectors) != VolumeKind::Unknown)
    {
        return false;
    }

    InfocomSave::FindPositions (sectors, positions);

    for (const InfocomSave::Position & position : positions)
    {
        outLines.push_back (std::format ("{}. {}", position.number + 1, InfocomSave::Describe (position)));
    }

    return !outLines.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveSummary::FindWizardry
//
//  An Apple Pascal volume's SCENARIO.DATA decoded as the viewer decodes it:
//  each character's first line from the roster's count on. A scenario with
//  no characters has no save.
//
////////////////////////////////////////////////////////////////////////////////

bool SaveSummary::FindWizardry (const std::vector<Byte> & sectors, std::vector<std::string> & outLines)
{
    HRESULT                   hr       = S_OK;
    std::unique_ptr<IVolume>  volume;
    FilePayload               payload;
    FileFacts                 facts;
    DecodedFile               file;
    std::wstring              error;
    bool                      isRoster = false;
    bool                      hasCount = false;



    outLines.clear();

    BAIL_OUT_IF (VolumeImage::DetectFilesystem (sectors) != VolumeKind::Pascal, S_OK);

    volume = VolumeImage::CreateVolume (VolumeKind::Pascal, sectors);
    BAIL_OUT_IF (volume == nullptr, S_OK);

    hr = volume->Read (FilePath::Parse ("SCENARIO.DATA"), payload);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    facts.volume = VolumeKind::Pascal;
    facts.name   = "SCENARIO.DATA";

    hr = WizardryScenario::Decode (payload.bytes, facts, DecodeOptions(), file, error);
    BAIL_OUT_IF (FAILED (hr), S_OK);

    for (const Paragraph & paragraph : file.paragraphs)
    {
        std::string  text;

        if (!paragraph.picture.empty())
        {
            continue;
        }

        for (const Run & run : paragraph.runs)
        {
            for (wchar_t ch : run.text)
            {
                text += (ch >= L' ' && ch < 0x7F) ? (char) ch : ' ';
            }
        }

        //  The roster begins at its count, "Saved game: 3 characters".
        if (!isRoster && text.starts_with ("Saved game: "))
        {
            isRoster = true;
            hasCount = !text.starts_with ("Saved game: 0 ");
            continue;
        }

        //  A character's first line is its name, level, race, class and
        //  alignment; its status and items follow, indented or below it.
        if (isRoster && !text.starts_with (" ") && text.find (" Level ") != std::string::npos)
        {
            size_t  gap   = text.find ("  ");
            size_t  rest  = (gap == std::string::npos) ? std::string::npos : text.find_first_not_of (' ', gap);
            size_t  level = 0;

            //  The name in a column, then L and the level, as BART  L2 Human Samurai, Good.
            if (rest != std::string::npos)
            {
                text = std::format ("{:<{}}{}", text.substr (0, gap), kNameColumn, text.substr (rest));
            }

            level = text.find ("Level ");

            if (level != std::string::npos)
            {
                text.replace (level, 6, "L");
            }

            outLines.push_back (text);
        }
    }

Error:
    return hasCount && !outLines.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  SaveSummary::Wrap
//
////////////////////////////////////////////////////////////////////////////////

void SaveSummary::Wrap (const std::string & line, std::vector<std::string> & ioRows)
{
    std::string  rest = line;



    while (rest.size() > kColumns)
    {
        size_t  space = rest.rfind (' ', kColumns);
        size_t  cut   = (space == std::string::npos || space == 0) ? kColumns : space;

        ioRows.push_back (rest.substr (0, cut));
        rest = rest.substr (rest[cut] == ' ' ? cut + 1 : cut);
    }

    ioRows.push_back (rest);
}
