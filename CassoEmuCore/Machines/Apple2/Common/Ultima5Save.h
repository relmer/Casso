#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save
//
//  An Ultima V Britannia disk, a ProDOS disk the game saves to, as the saved
//  game: the world map with the party marked, the characters, food, gold
//  and the date.
//
////////////////////////////////////////////////////////////////////////////////

class Ultima5Save
{
public:
    struct Character
    {
        std::wstring  name;
        bool          inParty        = false;
        bool          isFemale       = false;
        wchar_t       characterClass = L' ';
        wchar_t       status         = L' ';
        int           strength       = 0;
        int           dexterity      = 0;
        int           intelligence   = 0;
        int           magic          = 0;
        int           hitPoints      = 0;
        int           maxHitPoints   = 0;
        int           experience     = 0;
        int           level          = 0;
    };

    static constexpr size_t  kImageBytes  = 143360;
    static constexpr int     kWorldSide   = 256;
    static constexpr int     kCharacters  = 16;
    static constexpr size_t  kRosterBytes = 1024;
    static constexpr size_t  kPartyBytes  = 48;

    static bool          IsFound          (const std::vector<Byte> & sectors);
    static HRESULT       Decode           (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError);
    static bool          TryReadCharacter (std::span<const Byte> roster, int index, Character & outCharacter);
    static Byte          GetTile          (const std::vector<Byte> & sectors, int x, int y);
    static std::wstring  GetClassName     (wchar_t letter);
    static std::wstring  GetStatusName    (wchar_t letter);
    static std::wstring  GetTileName      (Byte tile);
    static uint32_t      GetTileColor     (Byte tile);

private:
    static constexpr int     kMapFirstBlock = 152;
    static constexpr int     kRegionSide    = 16;
    static constexpr size_t  kChunkBytes    = 256;
    static constexpr size_t  kNameBytes     = 8;
    static constexpr size_t  kRecordsAt     = 0x80;
    static constexpr size_t  kRecordBytes   = 16;
    static constexpr size_t  kFoodAt        = 0x180;
    static constexpr size_t  kGoldAt        = 0x182;

    static bool       TryReadFiles   (const std::vector<Byte> & sectors, std::vector<Byte> & outRoster, std::vector<Byte> & outParty);
    static int        ReadBcd        (std::span<const Byte> bytes, size_t at, size_t count);
    static bool       IsBcd          (Byte value);
    static void       RenderMap      (const std::vector<Byte> & sectors, std::span<const bool> unreadable, int partyX, int partyY, Paragraph & ioMap);
    static void       AddLegend      (std::vector<Paragraph> & ioParagraphs);
    static void       AddCharacter   (const Character & character, RecordTable & ioTable);
    static Run        MakeLabel      (const std::wstring & text);
    static Run        MakeValue      (const std::wstring & text);
    static RunColor   GetHealthColor (int hitPoints, int maxHitPoints);
    static Paragraph  MakeParagraph  (std::vector<Run> runs);
};
