#include "Pch.h"

#include "FileTypes/DiskSaveTable.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/InfocomSave.h"
#include "Machines/Apple2/Common/Ultima3Save.h"
#include "Machines/Apple2/Common/Ultima4Save.h"
#include "Machines/Apple2/Common/Ultima5Save.h"
#include "Machines/Apple2/Common/Ultima1Save.h"





static constexpr DiskSaveTable::Entry  s_kEntries[] =
{
    { &DiskSaveTable::IsInfocomSave, &DiskSaveTable::DecodeInfocomSave },
    { &DiskSaveTable::IsUltima3Save, &DiskSaveTable::DecodeUltima3Save },
    { &DiskSaveTable::IsUltima4Save, &DiskSaveTable::DecodeUltima4Save },
    { &DiskSaveTable::IsUltima5Save, &DiskSaveTable::DecodeUltima5Save },
    { &DiskSaveTable::IsUltima1Save, &DiskSaveTable::DecodeUltima1Save },
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
