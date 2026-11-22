#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save
//
//  An Ultima IV save disk, Disk C of the game, as the saved game: the world
//  map with the party marked, the characters, and the virtues. Measured
//  from the owner's Disk C, a DOS 3.3 disk:
//
//    tracks 0-15   the world, 256 by 256 tiles, as 16 by 16 regions of 16
//                  by 16 tiles, a region a sector in DOS logical order,
//                  outside the catalog
//    ROST          512 bytes: eight 32-byte characters, then the virtues
//    PRTY          32 bytes: the party's x and y on the world first
//
//  A character has its name in high-bit ASCII, its sex as the game's glyph
//  for it, its class from 0, its status as a letter, and its stats, hit
//  points and experience as binary-coded decimal, then its weapon and
//  armor.
//
////////////////////////////////////////////////////////////////////////////////

class Ultima4Save
{
public:
    struct Character
    {
        std::wstring  name;
        bool          isFemale     = false;
        int           classIndex   = 0;
        wchar_t       status       = L' ';
        int           strength     = 0;
        int           dexterity    = 0;
        int           intelligence = 0;
        int           magic        = 0;
        int           hitPoints    = 0;
        int           maxHitPoints = 0;
        int           experience   = 0;
        int           weapon       = 0;
        int           armor        = 0;
    };

    static constexpr size_t  kImageBytes   = 143360;
    static constexpr int     kWorldSide    = 256;
    static constexpr int     kCharacters   = 8;
    static constexpr int     kVirtues      = 8;
    static constexpr size_t  kRosterBytes  = 512;
    static constexpr size_t  kPartyBytes   = 32;

    //  Whether a 140K DOS 3.3 disk in DOS logical order is an Ultima IV save disk.
    static bool          IsFound          (const std::vector<Byte> & sectors);

    //  The saved game as a document. `unreadable` has a flag for each of the
    //  disk's 560 sectors, set for one that couldn't be read, or is empty.
    static HRESULT       Decode           (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError);

    static bool          TryReadCharacter (std::span<const Byte> record, Character & outCharacter);

    //  The tile at a place on the world, from its region's sector.
    static Byte          GetTile          (std::span<const Byte> sectors, int x, int y);

    static std::wstring  GetClassName     (int classIndex);
    static std::wstring  GetStatusName    (wchar_t letter);
    static std::wstring  GetWeaponName    (int weapon);
    static std::wstring  GetArmorName     (int armor);
    static std::wstring  GetTileName      (Byte tile);
    static uint32_t      GetTileColor     (Byte tile);

private:
    static constexpr size_t  kSectorBytes   = 256;
    static constexpr int     kRegionSide    = 16;
    static constexpr size_t  kRecordBytes   = 32;
    static constexpr size_t  kNameBytes     = 16;
    static constexpr size_t  kVirtuesAt     = 0x100;

    static bool                          TryReadFiles       (const std::vector<Byte> & sectors, std::vector<Byte> & outRoster, std::vector<Byte> & outParty);
    static int                           ReadBcd            (std::span<const Byte> bytes, size_t at, size_t count);
    static bool                          IsBcd              (Byte value);
    static void                          RenderMap          (std::span<const Byte> sectors, std::span<const bool> unreadable, int partyX, int partyY, Paragraph & ioMap);
    static void                          AddCharacter       (const Character & character, RecordTable & ioTable);
    static void                          AddLegend          (std::vector<Paragraph> & ioParagraphs);
    static void                          AddVirtues         (std::span<const Byte> roster, std::vector<Paragraph> & ioParagraphs);
    static Paragraph                     MakeParagraph      (std::vector<Run> runs);
    static Run                           MakeLabel          (const std::wstring & text);
    static Run                           MakeValue          (const std::wstring & text);
    static RunColor                      GetHealthColor     (int hitPoints, int maxHitPoints);
};
