#include "Pch.h"

#include "FileTypes/DiskSaveTable.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/InfocomSave.h"





static constexpr DiskSaveTable::Entry  s_kEntries[] =
{
    { &DiskSaveTable::IsInfocomSave, &DiskSaveTable::DecodeInfocomSave },
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
//  A heading, then each saved position: its number in bold and the game,
//  the room, and the score and moves or the time.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT DiskSaveTable::DecodeInfocomSave (const std::vector<Byte> & sectors, DecodedFile & outFile)
{
    HRESULT                             hr        = S_OK;
    std::vector<InfocomSave::Position>  positions;
    Paragraph                           heading;
    bool                                found     = false;



    InfocomSave::FindPositions (sectors, positions);
    found = !positions.empty();
    CBREx (found, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    outFile      = DecodedFile();
    outFile.kind = DecodedFile::Kind::Document;

    heading.runs = { Run { L"Infocom saved game", true } };
    outFile.paragraphs.push_back (heading);

    for (const InfocomSave::Position & position : positions)
    {
        Paragraph  line;

        line.runs = { Run { std::format (L"Position {}   ", position.number), true },
                      Run { TextEncoding::NarrowToWide (InfocomSave::Describe (position)) } };

        outFile.paragraphs.push_back (Paragraph());
        outFile.paragraphs.push_back (std::move (line));
    }

Error:
    return hr;
}
