#include "Pch.h"

#include "FileTypes/DiskSaveTable.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
#include "Devices/Disk/IDiskFileIo.h"
#include "Machines/Apple2/Common/InfocomSave.h"
#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/Ultima3Save.h"
#include "Machines/Apple2/Common/Ultima4Save.h"
#include "Machines/Apple2/Common/Ultima5Save.h"
#include "Machines/Apple2/Common/Ultima1Map.h"
#include "Machines/Apple2/Common/Ultima1Save.h"
#include "Machines/Apple2/Common/Ultima1981Save.h"
#include "Machines/Apple2/Common/Ultima2Save.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/SectorDecodeReport.h"





static constexpr DiskSaveTable::Entry  s_kEntries[] =
{
    { &DiskSaveTable::IsInfocomSave, &DiskSaveTable::DecodeInfocomSave },
    { &DiskSaveTable::IsUltima3Save, &DiskSaveTable::DecodeUltima3Save },
    { &DiskSaveTable::IsUltima4Save, &DiskSaveTable::DecodeUltima4Save },
    { &DiskSaveTable::IsUltima5Save, &DiskSaveTable::DecodeUltima5Save },
    { &DiskSaveTable::IsUltima1Save, &DiskSaveTable::DecodeUltima1Save, &DiskSaveTable::IsUltima1GameDisk, &DiskSaveTable::DecodeUltima1WithMap },
    { &DiskSaveTable::IsUltima1GameDisk, &DiskSaveTable::DecodeUltima1Game },
    { &DiskSaveTable::IsUltima1981Save, &DiskSaveTable::DecodeUltima1981Save },
    { &DiskSaveTable::IsUltima2Save, &DiskSaveTable::DecodeUltima2Save },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::Find
//
////////////////////////////////////////////////////////////////////////////////

const DiskSaveTable::Entry * DiskSaveTable::Find (const std::vector<Byte> & sectors)
{
    for (const Entry & entry : s_kEntries)
    {
        if (entry.isFound (sectors))
        {
            return &entry;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeImage
//
//  The image's sectors decoded by the entry that accepts them, with a flag
//  for each sector the track layer couldn't read. When the entry tests for a
//  companion and findCompanion finds one, the two are decoded together.
//  Fails for an image that no entry accepts.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeImage (const std::vector<Byte> & bytes, const std::string & path, const FindCompanionFn & findCompanion, DecodedFile & outFile)
{
    HRESULT                  hr           = S_OK;
    std::vector<Byte>        sectors;
    std::vector<Byte>        companion;
    bool                     hasCompanion = false;
    SectorDecodeReport       report;
    const Entry            * entry        = nullptr;
    size_t                   count        = 0;
    std::unique_ptr<bool[]>  lost;



    hr = VolumeImage::Load (bytes, path, sectors, report);
    CHR (hr);

    entry = Find (sectors);
    CBR (entry != nullptr);

    hasCompanion = entry->isCompanion != nullptr && findCompanion && findCompanion (entry->isCompanion, companion);
    BAIL_OUT_IF (hasCompanion, entry->decodeWithCompanion (sectors, companion, outFile));

    if (report.HasDataLoss())
    {
        count = (size_t) report.GetTrackCount() * NibbleFormat::kSectorsPerTrack;
        lost  = std::make_unique<bool[]> (count);

        for (size_t i = 0; i < count; i++)
        {
            int  track = (int) (i / NibbleFormat::kSectorsPerTrack);

            lost[i] = report.GetOutcome (track) == TrackDecodeOutcome::Partial && !report.IsSectorRecovered (track, (int) (i % NibbleFormat::kSectorsPerTrack));
        }
    }

    hr = entry->decode (sectors, std::span<const bool> (lost.get(), count), outFile);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::FindBeside
//
//  Each other file in path's folder, in name order, that loads as a disk
//  image.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::FindBeside (IDiskFileIo & io, const std::string & path, IsFoundFn isCompanion, std::vector<Byte> & outSectors)
{
    HRESULT                   hr    = S_OK;
    size_t                    slash = path.find_last_of ("\\/");
    std::string               folder;
    std::vector<std::string>  names;



    if (slash == std::string::npos)
    {
        return false;
    }

    folder = path.substr (0, slash);
    hr     = io.ListFiles (folder, names);

    if (FAILED (hr))
    {
        return false;
    }

    std::sort (names.begin(), names.end());

    for (const std::string & name : names)
    {
        std::string  candidate = folder + path[slash] + name;

        if (_stricmp (candidate.c_str(), path.c_str()) != 0 && TryLoadImage (io, candidate, outSectors) && isCompanion (outSectors))
        {
            return true;
        }
    }

    outSectors.clear();

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::TryLoadImage
//
//  The file's sectors when it loads as a disk image. A file larger than
//  kMostBytes is passed over, since a floppy's image is far smaller and
//  reading a hard disk's would stall the preview.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::TryLoadImage (IDiskFileIo & io, const std::string & path, std::vector<Byte> & outSectors)
{
    constexpr uint64_t  kMostBytes = 2 * 1024 * 1024;
    HRESULT             hr         = S_OK;
    FileStamp           stamp;
    std::vector<Byte>   bytes;
    SectorDecodeReport  report;



    hr = io.Stat (path, stamp);
    CHR (hr);

    CBREx (stamp.sizeBytes <= kMostBytes, HRESULT_FROM_WIN32 (ERROR_FILE_TOO_LARGE));

    hr = io.ReadAllBytes (path, bytes);
    CHR (hr);

    hr = VolumeImage::Load (bytes, path, outSectors, report);
    CHR (hr);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::IsInfocomSave
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::IsInfocomSave (const std::vector<Byte> & sectors)
{
    std::vector<InfocomSave::Position>  positions;



    InfocomSave::FindPositions (sectors, positions);

    return !positions.empty();
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeInfocomSave
//
//  The game's title and subtitle in bold as the heading, or "Infocom" when
//  the positions are of different games or one not in the table, with the
//  first position's release and serial. Then each saved position as a
//  table of a label and a value: the room, the score out of the game's
//  maximum with a bar, the moves or the time, then what the player has
//  and what's in the room, nested as they are.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeInfocomSave (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile)
{
    HRESULT                             hr        = S_OK;
    std::vector<InfocomSave::Position>  positions;
    Paragraph                           heading;
    bool                                found     = false;



    UNREFERENCED_PARAMETER (unreadable);

    InfocomSave::FindPositions (sectors, positions);
    found = !positions.empty();
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outFile      = DecodedFile();
    outFile.kind = DecodedFile::Kind::Document;

    heading.runs = { Run { TextEncoding::NarrowToWide (GetInfocomTitle (positions)), true },
                     Run { std::format (L", release {}, serial {}", positions[0].release, TextEncoding::NarrowToWide (positions[0].serial)) } };
    outFile.paragraphs.push_back (heading);

    for (size_t i = 0; i < positions.size(); i++)
    {
        //  A line between one position and the next.
        if (i > 0)
        {
            Paragraph  rule;

            rule.hasRule = true;
            outFile.paragraphs.push_back (rule);
        }

        AddInfocomPosition (positions[i], outFile.paragraphs);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::GetInfocomTitle
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskSaveTable::GetInfocomTitle (const std::vector<InfocomSave::Position> & positions)
{
    std::string  title;
    bool         isSame = true;



    for (const InfocomSave::Position & position : positions)
    {
        const InfocomSave::Story *  story = InfocomSave::FindStory (position.release, position.serial);
        std::string                 next  = story ? story->title : std::string();

        isSame = isSame && !next.empty() && (title.empty() || title == next);
        title  = next;
    }

    return isSame ? InfocomSave::GetFullTitle (title) : std::string ("Infocom");
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::AddInfocomPosition
//
////////////////////////////////////////////////////////////////////////////////

void DiskSaveTable::AddInfocomPosition (const InfocomSave::Position & position, std::vector<Paragraph> & ioParagraphs)
{
    const InfocomSave::Story *  story = InfocomSave::FindStory (position.release, position.serial);
    int                         hours = (position.score % 12 == 0) ? 12 : position.score % 12;
    Paragraph                   line;



    ioParagraphs.push_back (Paragraph());
    line.runs = { Run { L"Save position\t", false, false, false, RunColor::Muted }, Run { std::format (L"{}", position.number), true } };
    ioParagraphs.push_back (line);

    line.runs = { Run { L"Room\t", false, false, false, RunColor::Muted }, Run { position.room.empty() ? std::wstring (L"Unknown") : position.room, true } };
    ioParagraphs.push_back (line);

    //  A later story's status line isn't in fixed globals, so its rows are left out.
    if (position.hasStatus && position.isTimed)
    {
        line.runs = { Run { L"Time\t", false, false, false, RunColor::Muted },
                      Run { std::format (L"{}:{:02} {}", hours, position.moves, (position.score < 12) ? L"AM" : L"PM"), true, false, false, RunColor::Keyword } };
        ioParagraphs.push_back (line);
    }
    else if (position.hasStatus)
    {
        std::string  title = story ? story->title : std::string();

        if (position.hasScore && !InfocomSave::IsUnscored (title))
        {
            line.runs = { Run { L"Score\t", false, false, false, RunColor::Muted }, Run { std::format (L"{}", position.score), true, false, false, RunColor::Keyword } };

            if (story != nullptr && story->maxScore > 0)
            {
                line.runs.push_back (Run { std::format (L" of {}   ", story->maxScore), false, false, false, RunColor::Muted });
                AddScoreBar (position.score, story->maxScore, line.runs);
            }

            ioParagraphs.push_back (line);
        }

        line.runs = { Run { std::wstring (InfocomSave::GetMovesLabel (title)) + L"\t", false, false, false, RunColor::Muted },
                      Run { std::format (L"{}", position.moves), true, false, false, RunColor::Keyword } };
        ioParagraphs.push_back (line);
    }

    AddInfocomItems (L"Carrying", position.carried, ioParagraphs);
    AddInfocomItems (L"Here",     position.here,    ioParagraphs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::AddInfocomItems
//
//  A blank line, then the label in gray beside the first item, and the
//  other items under it, each indented under what it's in.
//
////////////////////////////////////////////////////////////////////////////////

void DiskSaveTable::AddInfocomItems (const wchar_t * heading, const std::vector<InfocomSave::Item> & items, std::vector<Paragraph> & ioParagraphs)
{
    Paragraph  line;



    ioParagraphs.push_back (Paragraph());

    if (items.empty())
    {
        line.runs = { Run { std::wstring (heading) + L"\t", false, false, false, RunColor::Muted }, Run { L"Nothing", false, true, false, RunColor::Muted } };
        ioParagraphs.push_back (line);
    }

    for (size_t i = 0; i < items.size(); i++)
    {
        line.runs = { Run { std::wstring ((i == 0) ? heading : L"") + L"\t", false, false, false, RunColor::Muted },
                      Run { std::wstring ((size_t) items[i].depth * s_kItemIndent, L' ') + items[i].name } };
        ioParagraphs.push_back (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::AddScoreBar
//
//  A solid line s_kScoreBarCells characters long: green for the score,
//  rounded up so any score is visible, and gray for the rest of the maximum.
//
////////////////////////////////////////////////////////////////////////////////

void DiskSaveTable::AddScoreBar (int score, int maxScore, std::vector<Run> & ioRuns)
{
    int           filled = ((std::clamp) (score, 0, maxScore) * s_kScoreBarCells + maxScore - 1) / maxScore;
    std::wstring  done;
    std::wstring  rest;



    for (int i = 0; i < s_kScoreBarCells; i++)
    {
        (i < filled ? done : rest) += s_kpszHeavyLine;
    }

    ioRuns.push_back (Run { done, false, false, false, RunColor::Good });
    ioRuns.push_back (Run { rest, false, false, false, RunColor::Muted });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::IsUltima3Save
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::IsUltima3Save (const std::vector<Byte> & sectors)
{
    return Ultima3Save::IsFound (sectors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeUltima3Save
//
//  The player disk's saved game, with the map rows an unreadable sector
//  covers striped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeUltima3Save (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile)
{
    std::wstring  error;



    return Ultima3Save::Decode (sectors, unreadable, outFile, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::IsUltima4Save
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::IsUltima4Save (const std::vector<Byte> & sectors)
{
    return Ultima4Save::IsFound (sectors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeUltima4Save
//
//  The save disk's saved game, with the world's tiles an unreadable sector
//  covers striped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeUltima4Save (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile)
{
    std::wstring  error;



    return Ultima4Save::Decode (sectors, unreadable, outFile, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::IsUltima5Save
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::IsUltima5Save (const std::vector<Byte> & sectors)
{
    return Ultima5Save::IsFound (sectors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeUltima5Save
//
//  The Britannia disk's saved game, with the world's tiles an unreadable
//  sector covers striped.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeUltima5Save (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile)
{
    std::wstring  error;



    return Ultima5Save::Decode (sectors, unreadable, outFile, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::IsUltima1Save
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::IsUltima1Save (const std::vector<Byte> & sectors)
{
    return Ultima1Save::IsFound (sectors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeUltima1Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeUltima1Save (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile)
{
    std::wstring  error;



    return Ultima1Save::Decode (sectors, unreadable, outFile, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeUltima1WithMap
//
//  The player disk's saved game, with its continent from the game disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeUltima1WithMap (const std::vector<Byte> & sectors, const std::vector<Byte> & game, DecodedFile & outFile)
{
    std::wstring  error;



    return Ultima1Save::DecodeWithMap (sectors, &game, outFile, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::IsUltima1GameDisk
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::IsUltima1GameDisk (const std::vector<Byte> & sectors)
{
    return Ultima1Map::IsGameDisk (sectors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeUltima1Game
//
//  The game disk's four continents.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeUltima1Game (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile)
{
    UNREFERENCED_PARAMETER (unreadable);

    return Ultima1Map::DecodeGameDisk (sectors, outFile);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::IsUltima2Save
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::IsUltima2Save (const std::vector<Byte> & sectors)
{
    return Ultima2Save::IsFound (sectors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeUltima2Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeUltima2Save (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile)
{
    std::wstring  error;



    return Ultima2Save::Decode (sectors, unreadable, outFile, error);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::IsUltima1981Save
//
////////////////////////////////////////////////////////////////////////////////

bool DiskSaveTable::IsUltima1981Save (const std::vector<Byte> & sectors)
{
    return Ultima1981Save::IsFound (sectors);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::DecodeUltima1981Save
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeUltima1981Save (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile)
{
    std::wstring  error;



    return Ultima1981Save::Decode (sectors, unreadable, outFile, error);
}
