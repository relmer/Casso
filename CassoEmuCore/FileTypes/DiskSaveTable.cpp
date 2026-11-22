#include "Pch.h"

#include "FileTypes/DiskSaveTable.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/InfocomSave.h"
#include "Machines/Apple2/Common/Ultima3Save.h"
#include "Machines/Apple2/Common/Ultima4Save.h"





static constexpr DiskSaveTable::Entry  s_kEntries[] =
{
    { &DiskSaveTable::IsInfocomSave, &DiskSaveTable::DecodeInfocomSave },
    { &DiskSaveTable::IsUltima3Save, &DiskSaveTable::DecodeUltima3Save },
    { &DiskSaveTable::IsUltima4Save, &DiskSaveTable::DecodeUltima4Save },
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
//  The game's title and subtitle as the heading, or "Infocom" when the
//  positions are of different games or one not in the table, then each
//  saved position: its number and the game in bold,
//  the release and serial in gray, the room, the score and moves or the
//  time, a bar of the score out of the game's maximum, then what the player
//  carries and what's in the room, nested as they are.
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

    heading.runs = { Run { TextEncoding::NarrowToWide (GetInfocomTitle (positions)) + L" saved game", true } };
    outFile.paragraphs.push_back (heading);

    for (const InfocomSave::Position & position : positions)
    {
        AddInfocomPosition (position, outFile.paragraphs);
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
    std::wstring                title = story ? TextEncoding::NarrowToWide (story->title) : std::wstring (L"Unknown story");
    int                         hours = (position.score % 12 == 0) ? 12 : position.score % 12;
    Paragraph                   line;



    ioParagraphs.push_back (Paragraph());
    line.runs = { Run { std::format (L"Position {}   {}", position.number, title), true },
                  Run { std::format (L"   release {}, serial {}", position.release, TextEncoding::NarrowToWide (position.serial)), false, false, false, RunColor::Muted } };
    ioParagraphs.push_back (line);

    line.runs = { Run { L"Room   ", false, false, false, RunColor::Muted }, Run { position.room.empty() ? std::wstring (L"Unknown") : position.room, true } };
    ioParagraphs.push_back (line);

    if (position.isTimed)
    {
        line.runs = { Run { L"Time   ", false, false, false, RunColor::Muted },
                      Run { std::format (L"{}:{:02} {}", hours, position.moves, (position.score < 12) ? L"AM" : L"PM"), true, false, false, RunColor::Keyword } };
        ioParagraphs.push_back (line);
    }
    else
    {
        line.runs = { Run { L"Score   ", false, false, false, RunColor::Muted }, Run { std::format (L"{}", position.score), true, false, false, RunColor::Keyword } };

        if (story != nullptr && story->maxScore > 0)
        {
            line.runs.push_back (Run { std::format (L" of {}", story->maxScore), false, false, false, RunColor::Muted });
        }

        line.runs.push_back (Run { L"   Moves   ", false, false, false, RunColor::Muted });
        line.runs.push_back (Run { std::format (L"{}", position.moves), true, false, false, RunColor::Keyword });
        ioParagraphs.push_back (line);

        if (story != nullptr && story->maxScore > 0)
        {
            ioParagraphs.push_back (MakeScoreBar (position.score, story->maxScore));
        }
    }

    AddInfocomItems (L"Carrying", position.carried, ioParagraphs);
    AddInfocomItems (L"Here",     position.here,    ioParagraphs);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::AddInfocomItems
//
//  A heading in gray, then the items, each indented under what it's in.
//
////////////////////////////////////////////////////////////////////////////////

void DiskSaveTable::AddInfocomItems (const wchar_t * heading, const std::vector<InfocomSave::Item> & items, std::vector<Paragraph> & ioParagraphs)
{
    Paragraph  line;



    line.runs = { Run { heading, true, false, false, RunColor::Muted } };
    ioParagraphs.push_back (line);

    if (items.empty())
    {
        line.runs = { Run { L"   Nothing", false, true, false, RunColor::Muted } };
        ioParagraphs.push_back (line);
    }

    for (const InfocomSave::Item & item : items)
    {
        line.runs = { Run { std::wstring (3 + (size_t) item.depth * 5, L' ') + item.name } };
        ioParagraphs.push_back (line);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSaveTable::MakeScoreBar
//
//  A bar s_kScoreBarWidth pixels wide, green for the score and gray for the
//  rest of the maximum.
//
////////////////////////////////////////////////////////////////////////////////

Paragraph DiskSaveTable::MakeScoreBar (int score, int maxScore)
{
    Paragraph  bar;
    int        filled = (std::clamp) (score, 0, maxScore) * s_kScoreBarWidth / maxScore;



    bar.width  = s_kScoreBarWidth;
    bar.height = s_kScoreBarHeight;
    bar.picture.assign ((size_t) bar.width * (size_t) bar.height, s_kScoreBarRest);

    for (int y = 0; y < bar.height; y++)
    {
        std::fill_n (bar.picture.begin() + (ptrdiff_t) y * bar.width, filled, s_kScoreBarFill);
    }

    return bar;
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