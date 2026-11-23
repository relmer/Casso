#include "Pch.h"

#include "Machines/Apple2/Common/Ultima2Save.h"
#include "Machines/Apple2/Common/Dos33Volume.h"





//  The map's tiles, each a multiple of 4, with the color the map draws it in
//  and its name in a tip. Water, grass, forest and mountains were checked
//  against the game's screen around the player. A number not in the table
//  is a place: a town, castle, dungeon, tower or signpost.
static constexpr struct { Byte tile; uint32_t argb; const wchar_t * pszName; } s_kTiles[] =
{
    { 0x00, 0xFF2846AA, L"Water"     },
    { 0x04, 0xFF50643C, L"Swamp"     },
    { 0x08, 0xFF5AAA46, L"Grass"     },
    { 0x0C, 0xFF1E692D, L"Forest"    },
    { 0x10, 0xFF7D7369, L"Mountains" },
};

static constexpr uint32_t  s_kPlaceTile  = 0xFFDCBE3C;
static constexpr uint32_t  s_kPartyMark  = 0xFFFFFFFF;
static constexpr uint32_t  s_kPartyEdge  = 0xFF000000;
static constexpr int       s_kTilePixels = 6;

//  The inventory's counts, a decimal byte each, by name: weapons from $45,
//  armor from $65 and items from $A4. These are the PC version's offsets
//  plus 4, the shift every field checked in the game has.
static constexpr const wchar_t * s_kWeapons[] = { L"Dagger", L"Mace", L"Axe", L"Bow", L"Sword", L"Great sword", L"Light sword", L"Phaser", L"Quicksword" };
static constexpr const wchar_t * s_kArmor[]   = { L"Cloth", L"Leather", L"Chain", L"Plate", L"Reflect", L"Power" };
static constexpr const wchar_t * s_kItems[]   = { L"Rings", L"Wand", L"Staff", L"Boots", L"Cloak", L"Helm", L"Gem", L"Ankh",
                                                  L"Red gem", L"Skull key", L"Green gem", L"Brass button", L"Blue tassle",
                                                  L"Strange coin", L"Green idol", L"Tri-lithium" };





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::IsBcd
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima2Save::IsBcd (Byte value)
{
    constexpr Byte  kDigitMax = 9;
    constexpr int   kNibble   = 4;
    constexpr Byte  kLowMask  = 0x0F;



    return (value >> kNibble) <= kDigitMax && (value & kLowMask) <= kDigitMax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::ReadBcd
//
//  `count` bytes of binary-coded decimal at `at`, the most significant
//  first: 03 99 as 399.
//
////////////////////////////////////////////////////////////////////////////////

int Ultima2Save::ReadBcd (std::span<const Byte> bytes, size_t at, size_t count)
{
    constexpr int   kNibble  = 4;
    constexpr int   kTen     = 10;
    constexpr int   kHundred = 100;
    constexpr Byte  kLowMask = 0x0F;
    int             value    = 0;



    for (size_t i = 0; i < count; i++)
    {
        Byte  b = bytes[at + i];

        value = value * kHundred + (b >> kNibble) * kTen + (b & kLowMask);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::TryParse
//
//  The name from $04 in high-bit letters ending at a zero; the sex at $14,
//  a high-bit F or M; the class at $15 and the race at $16, each from 0;
//  Str, Agi, Sta, Cha, Wis and Int from $19, a decimal byte each; then hit
//  points at $1F, food at $21 and gold at $26, two decimal bytes each; and
//  the column and row at $28 and $29. False for a sector that isn't a
//  character.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima2Save::TryParse (std::span<const Byte> record, Character & outCharacter)
{
    constexpr Byte    kHighBit    = 0x80;
    constexpr Byte    kFemale     = 'F' | 0x80;
    constexpr Byte    kMale       = 'M' | 0x80;
    constexpr Byte    kFirstGlyph = 0xC1;
    constexpr Byte    kLastGlyph  = 0xDA;
    constexpr size_t  kNameAt     = 0x04;
    constexpr size_t  kNameBytes  = 16;
    constexpr size_t  kSexAt      = 0x14;
    constexpr size_t  kClassAt    = 0x15;
    constexpr size_t  kRaceAt     = 0x16;
    constexpr size_t  kStatsAt    = 0x19;
    constexpr size_t  kStats      = 6;
    constexpr size_t  kHitsAt     = 0x1F;
    constexpr size_t  kFoodAt     = 0x21;
    constexpr size_t  kGoldAt     = 0x26;
    constexpr size_t  kXAt        = 0x28;
    constexpr size_t  kYAt        = 0x29;
    constexpr size_t  kPeriodAt   = 0x17;
    constexpr size_t  kAreaAt     = 0x18;
    constexpr size_t  kExpAt      = 0x24;
    constexpr size_t  kTorchesAt  = 0x32;
    constexpr size_t  kKeysAt     = 0x33;
    constexpr size_t  kToolsAt    = 0x34;
    constexpr size_t  kWeaponsAt  = 0x45;
    constexpr size_t  kArmorAt    = 0x65;
    constexpr size_t  kItemsAt    = 0xA4;
    constexpr size_t  kWordBytes  = 2;
    constexpr int     kKinds      = 4;
    bool              isValid     = record.size() >= kRecordBytes
                                    && record[kNameAt] >= kFirstGlyph && record[kNameAt] <= kLastGlyph
                                    && (record[kSexAt] == kFemale || record[kSexAt] == kMale)
                                    && record[kClassAt] < kKinds && record[kRaceAt] < kKinds;



    for (size_t i = 0; isValid && i < kStats; i++)
    {
        isValid = IsBcd (record[kStatsAt + i]);
    }

    for (size_t at : { kHitsAt, kFoodAt, kGoldAt })
    {
        isValid = isValid && IsBcd (record[at]) && IsBcd (record[at + 1]);
    }

    if (!isValid)
    {
        return false;
    }

    outCharacter = Character();

    for (size_t i = 0; i < kNameBytes && record[kNameAt + i] != 0; i++)
    {
        outCharacter.name += (wchar_t) (record[kNameAt + i] & ~kHighBit);
    }

    outCharacter.isFemale       = record[kSexAt] == kFemale;
    outCharacter.characterClass = record[kClassAt];
    outCharacter.race           = record[kRaceAt];
    outCharacter.strength       = ReadBcd (record, kStatsAt,     1);
    outCharacter.agility        = ReadBcd (record, kStatsAt + 1, 1);
    outCharacter.stamina        = ReadBcd (record, kStatsAt + 2, 1);
    outCharacter.charisma       = ReadBcd (record, kStatsAt + 3, 1);
    outCharacter.wisdom         = ReadBcd (record, kStatsAt + 4, 1);
    outCharacter.intelligence   = ReadBcd (record, kStatsAt + 5, 1);
    outCharacter.hitPoints      = ReadBcd (record, kHitsAt, kWordBytes);
    outCharacter.food           = ReadBcd (record, kFoodAt, kWordBytes);
    outCharacter.gold           = ReadBcd (record, kGoldAt, kWordBytes);
    outCharacter.experience     = ReadBcd (record, kExpAt, kWordBytes);
    outCharacter.x              = record[kXAt];
    outCharacter.y              = record[kYAt];
    outCharacter.period         = record[kPeriodAt];
    outCharacter.area           = record[kAreaAt];
    outCharacter.torches        = ReadBcd (record, kTorchesAt, 1);
    outCharacter.keys           = ReadBcd (record, kKeysAt,    1);
    outCharacter.tools          = ReadBcd (record, kToolsAt,   1);

    AddInventory (record, kWeaponsAt, s_kWeapons, outCharacter);
    AddInventory (record, kArmorAt,   s_kArmor,   outCharacter);
    AddInventory (record, kItemsAt,   s_kItems,   outCharacter);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::AddInventory
//
//  Each name whose count, a decimal byte from `at` on, isn't zero.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima2Save::AddInventory (std::span<const Byte> record, size_t at, std::span<const wchar_t * const> names, Character & ioCharacter)
{
    for (size_t i = 0; i < names.size(); i++)
    {
        int  count = ReadBcd (record, at + i, 1);

        if (count > 0)
        {
            ioCharacter.inventory.emplace_back (names[i], count);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::TryReadMap
//
//  The 64 by 64 tiles of the file MAP <period><area>. The disk's catalog
//  starts at track 17 sector 1, where the game moved it, so a copy with
//  that sector also at sector 0 is read.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima2Save::TryReadMap (const std::vector<Byte> & sectors, int period, int area, std::vector<Byte> & outMap)
{
    constexpr size_t   kSectorBytes  = 256;
    constexpr size_t   kCatalogTrack = 17;
    constexpr size_t   kTrackBytes   = 16 * kSectorBytes;
    constexpr size_t   kMapBytes     = (size_t) kMapSide * kMapSide;
    HRESULT            hr            = S_OK;
    std::vector<Byte>  copy          = sectors;
    FilePayload        payload;
    bool               isSized       = false;



    std::copy_n (sectors.begin() + (ptrdiff_t) (kCatalogTrack * kTrackBytes + kSectorBytes), kSectorBytes, copy.begin() + (ptrdiff_t) (kCatalogTrack * kTrackBytes));

    hr = Dos33Volume (copy).Read (FilePath::Parse (std::format ("MAP {}{}", period, area)), payload);
    CHR (hr);

    isSized = payload.bytes.size() >= kMapBytes;
    CBR (isSized);

    outMap.assign (payload.bytes.begin(), payload.bytes.begin() + (ptrdiff_t) kMapBytes);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::GetTileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima2Save::GetTileName (Byte tile)
{
    for (const auto & known : s_kTiles)
    {
        if (known.tile == tile)
        {
            return known.pszName;
        }
    }

    return L"Place";
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::GetTileColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Ultima2Save::GetTileColor (Byte tile)
{
    for (const auto & known : s_kTiles)
    {
        if (known.tile == tile)
        {
            return known.argb;
        }
    }

    return s_kPlaceTile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::RenderMap
//
//  Each tile s_kTilePixels square in its color, with a tip of its column,
//  row and name, and the player a white square with a black edge.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima2Save::RenderMap (const std::vector<Byte> & map, int partyX, int partyY, Paragraph & ioMap)
{
    int  side = kMapSide * s_kTilePixels;



    ioMap.width      = side;
    ioMap.height     = side;
    ioMap.tipCell    = s_kTilePixels;
    ioMap.tipLeft    = 0;
    ioMap.tipTop     = 0;
    ioMap.tipColumns = kMapSide;
    ioMap.pingX      = partyX * s_kTilePixels + s_kTilePixels / 2;
    ioMap.pingY      = partyY * s_kTilePixels + s_kTilePixels / 2;
    ioMap.canZoom    = true;
    ioMap.picture.assign ((size_t) side * (size_t) side, s_kPlaceTile);
    ioMap.tips.assign ((size_t) kMapSide * (size_t) kMapSide, PictureTip());

    for (int y = 0; y < kMapSide; y++)
    {
        for (int x = 0; x < kMapSide; x++)
        {
            Byte          tile    = map[(size_t) y * kMapSide + (size_t) x];
            bool          isParty = x == partyX && y == partyY;
            PictureTip  & tip     = ioMap.tips[(size_t) y * kMapSide + (size_t) x];

            for (int py = 0; py < s_kTilePixels; py++)
            {
                for (int px = 0; px < s_kTilePixels; px++)
                {
                    bool      isEdge = py == 0 || px == 0 || py == s_kTilePixels - 1 || px == s_kTilePixels - 1;
                    uint32_t  color  = isParty ? (isEdge ? s_kPartyEdge : s_kPartyMark) : GetTileColor (tile);

                    ioMap.picture[(size_t) (y * s_kTilePixels + py) * (size_t) side + (size_t) (x * s_kTilePixels + px)] = color;
                }
            }

            tip.lines.push_back ({ 0, std::format (L"Column {}, row {}", x, y) });

            if (isParty)
            {
                tip.lines.push_back ({ s_kPartyMark, L"The player" });
            }

            tip.lines.push_back ({ GetTileColor (tile), GetTileName (tile) });
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::AddLegend
//
////////////////////////////////////////////////////////////////////////////////

void Ultima2Save::AddLegend (std::vector<Paragraph> & ioParagraphs)
{
    Paragraph  legend;



    legend.runs.push_back (Run { L"Player", false, false, false, RunColor::Muted, s_kPartyMark });

    for (const auto & known : s_kTiles)
    {
        legend.runs.push_back (Run { known.pszName, false, false, false, RunColor::Muted, known.argb });
    }

    legend.runs.push_back (Run { L"Place", false, false, false, RunColor::Muted, s_kPlaceTile });
    legend.isLegend = true;
    ioParagraphs.push_back (std::move (legend));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima2Save::IsFound (const std::vector<Byte> & sectors)
{
    Character  character;



    return sectors.size() == kImageBytes
        && TryParse (std::span<const Byte> (sectors).subspan (kRecordAt, kRecordBytes), character);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::GetRaceName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima2Save::GetRaceName (int race)
{
    switch (race)
    {
        case 0:  return L"Human";
        case 1:  return L"Elf";
        case 2:  return L"Dwarf";
        case 3:  return L"Hobbit";
        default: return std::format (L"Race {}", race);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::GetClassName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima2Save::GetClassName (int characterClass)
{
    switch (characterClass)
    {
        case 0:  return L"Fighter";
        case 1:  return L"Cleric";
        case 2:  return L"Wizard";
        case 3:  return L"Thief";
        default: return std::format (L"Class {}", characterClass);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::MakeParagraph
//
////////////////////////////////////////////////////////////////////////////////

Paragraph Ultima2Save::MakeParagraph (std::vector<Run> runs)
{
    Paragraph  paragraph;



    paragraph.runs = std::move (runs);

    return paragraph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::MakeLabel
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima2Save::MakeLabel (const std::wstring & text)
{
    return Run { text, false, false, false, RunColor::Muted };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::MakeValue
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima2Save::MakeValue (const std::wstring & text)
{
    return Run { text, true, false, false, RunColor::Keyword };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::Decode
//
//  The title and position, the map with the player marked when the disk
//  has it, then the character: name, race, class and sex, hit points, food,
//  gold and experience, the six stats, and the inventory.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ultima2Save::Decode (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT            hr       = S_OK;
    Character          c;
    std::vector<Byte>  tiles;
    Paragraph          map;
    std::wstring       items;
    bool               isParsed = sectors.size() == kImageBytes
                                  && TryParse (std::span<const Byte> (sectors).subspan (kRecordAt, kRecordBytes), c);



    UNREFERENCED_PARAMETER (unreadable);

    outFile        = DecodedFile();
    outFile.format = FileFormat::UltimaSave;
    outFile.kind   = DecodedFile::Kind::Document;

    CBRFEx (isParsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The disk doesn't have an Ultima II saved game.");

    outFile.paragraphs.push_back (MakeParagraph ({ Run { L"Ultima II: Revenge of the Enchantress", true } }));
    outFile.paragraphs.push_back (MakeParagraph ({ Run { std::format (L"Saved game, the player at column {}, row {}", c.x, c.y), false, false, false, RunColor::Muted } }));
    if (TryReadMap (sectors, c.period, c.area, tiles))
    {
        RenderMap (tiles, c.x, c.y, map);
        outFile.paragraphs.push_back (std::move (map));
        AddLegend (outFile.paragraphs);
    }

    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (MakeParagraph ({ Run { c.name, true },
                                                   Run { std::format (L"   {} {}, {}", GetRaceName (c.race), GetClassName (c.characterClass), c.isFemale ? L"female" : L"male"), false, true, false, RunColor::Muted } }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"HP "),      MakeValue (std::format (L"{}", c.hitPoints)),
                                                   MakeLabel (L"   Food "), MakeValue (std::format (L"{}", c.food)),
                                                   MakeLabel (L"   Gold "), MakeValue (std::format (L"{}", c.gold)),
                                                   MakeLabel (L"   Exp "),  MakeValue (std::format (L"{}", c.experience)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Str "),     MakeValue (std::format (L"{}", c.strength)),
                                                   MakeLabel (L"   Agi "),  MakeValue (std::format (L"{}", c.agility)),
                                                   MakeLabel (L"   Sta "),  MakeValue (std::format (L"{}", c.stamina)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Cha "),     MakeValue (std::format (L"{}", c.charisma)),
                                                   MakeLabel (L"   Wis "),  MakeValue (std::format (L"{}", c.wisdom)),
                                                   MakeLabel (L"   Int "),  MakeValue (std::format (L"{}", c.intelligence)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Torches "), MakeValue (std::format (L"{}", c.torches)),
                                                   MakeLabel (L"   Keys "), MakeValue (std::format (L"{}", c.keys)),
                                                   MakeLabel (L"   Tools "), MakeValue (std::format (L"{}", c.tools)) }));

    for (const auto & [name, count] : c.inventory)
    {
        items += std::format (L"{}{} {}", items.empty() ? L"" : L", ", count, name);
    }

    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Inventory "), MakeValue (items.empty() ? L"none" : items) }));

Error:
    return hr;
}
