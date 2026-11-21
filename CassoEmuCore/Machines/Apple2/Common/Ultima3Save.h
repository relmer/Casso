#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save
//
//  An Ultima III player disk, which has no catalog, as the saved game: the
//  world map with the party marked, the characters in the party, and the
//  rest of the roster. Measured from two player disks and the scenario
//  side of the original disk, in DOS logical sector order:
//
//    track 3 sector 5      the party: the transport's tile, the party's
//                          size, its x and y on the map, and the roster
//                          slot of each member, counting from 1
//    track 3 sector 6      the members' characters, in party order
//    track 3 sectors 7-11  the roster, 20 characters
//    track 3 sector 13 to  the world map, 64 by 64 tiles, a row at a time
//    track 4 sector 12
//
//  A character is 64 bytes: the name in high-bit ASCII, the status, race,
//  class and sex as letters, and the stats, hit points, experience, food
//  and gold as binary-coded decimal.
//
////////////////////////////////////////////////////////////////////////////////

class Ultima3Save
{
public:
    struct Character
    {
        std::wstring  name;
        wchar_t       status         = L' ';
        wchar_t       race           = L' ';
        wchar_t       characterClass = L' ';
        wchar_t       sex            = L' ';
        int           strength       = 0;
        int           dexterity      = 0;
        int           intelligence   = 0;
        int           wisdom         = 0;
        int           magic          = 0;
        int           hitPoints      = 0;
        int           maxHitPoints   = 0;
        int           experience     = 0;
        int           food           = 0;
        int           gold           = 0;
        int           armor          = 0;
        int           weapon         = 0;
    };

    struct Party
    {
        int               x     = 0;
        int               y     = 0;
        int               size  = 0;
        std::vector<int>  slots;            // each member's roster slot, from 0
    };

    static constexpr size_t  kImageBytes  = 143360;
    static constexpr int     kMapSide     = 64;
    static constexpr int     kRosterSlots = 20;
    static constexpr int     kMaxParty    = 4;

    //  Whether a 140K disk in DOS logical order is an Ultima III player disk.
    static bool          IsFound          (std::span<const Byte> sectors);

    //  The saved game as a document. `unreadable` has a flag for each of the
    //  disk's 560 sectors, set for one that couldn't be read, or is empty.
    static HRESULT       Decode           (std::span<const Byte> sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError);

    static bool          TryReadParty     (std::span<const Byte> sectors, Party & outParty);
    static bool          TryReadCharacter (std::span<const Byte> record, Character & outCharacter);

    //  The map in 32-bit BGRA pixels, the party's tile marked and each row
    //  an unreadable sector covers hatched.
    static void          RenderMap        (std::span<const Byte> sectors, std::span<const bool> unreadable, int partyX, int partyY, std::vector<uint32_t> & outBgra, int & outWidth, int & outHeight);

    static std::wstring  GetRaceName      (wchar_t letter);
    static std::wstring  GetClassName     (wchar_t letter);
    static std::wstring  GetStatusName    (wchar_t letter);
    static std::wstring  GetArmorName     (int armor);
    static std::wstring  GetWeaponName    (int weapon);

private:
    static constexpr int     kSectorsPerTrack = 16;
    static constexpr size_t  kSectorBytes     = 256;
    static constexpr size_t  kRecordBytes     = 64;
    static constexpr size_t  kNameBytes       = 14;
    static constexpr size_t  kPartyAt         = (3 * kSectorsPerTrack + 5)  * kSectorBytes;
    static constexpr size_t  kMembersAt       = (3 * kSectorsPerTrack + 6)  * kSectorBytes;
    static constexpr size_t  kRosterAt        = (3 * kSectorsPerTrack + 7)  * kSectorBytes;
    static constexpr size_t  kMapAt           = (3 * kSectorsPerTrack + 13) * kSectorBytes;
    static constexpr size_t  kMapBytes        = (size_t) kMapSide * kMapSide;

    static int        ReadBcd       (std::span<const Byte> bytes, size_t at, size_t count);
    static bool       IsBcd         (Byte value);
    static void       AddCharacter  (const Character & character, std::vector<Paragraph> & ioParagraphs);
    static void       AddLegend     (std::vector<Paragraph> & ioParagraphs);
    static Paragraph  MakeParagraph (std::vector<Run> runs);
};
