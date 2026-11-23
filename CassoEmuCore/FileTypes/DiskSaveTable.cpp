#include "Pch.h"

#include "FileTypes/DiskSaveTable.h"
#include "Core/TextEncoding.h"
#include "Core/UnicodeSymbols.h"
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
//  other items under it in bold, each indented under what it's in.
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
                      Run { std::wstring ((size_t) items[i].depth * s_kItemIndent, L' ') + items[i].name, true } };
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