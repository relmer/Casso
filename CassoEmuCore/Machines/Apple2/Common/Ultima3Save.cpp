#include "Pch.h"

#include "Machines/Apple2/Common/Ultima3Save.h"





//  Each terrain tile's color in ARGB, by its tile number divided by four,
//  and its name in the legend. A tile past the end of the table, a monster
//  or a ship drawn on the map, is drawn in s_kOtherTile.
static constexpr struct { uint32_t argb; const wchar_t * pszColor; const wchar_t * pszName; } s_kTerrain[] =
{
    { 0xFF2846A0, L"Blue",        L"water"             },
    { 0xFF5AA046, L"Green",       L"grass"             },
    { 0xFF96AA50, L"Olive",       L"brush"             },
    { 0xFF1E692D, L"Dark green",  L"forest"            },
    { 0xFF7D7369, L"Gray",        L"mountains"         },
    { 0xFF3C2819, L"Brown",       L"dungeon entrance"  },
    { 0xFFDCBE3C, L"Gold",        L"town"              },
    { 0xFFF0F0F0, L"White",       L"castle"            },
};

static constexpr uint32_t        s_kOtherTile   = 0xFFC850C8;
static constexpr uint32_t        s_kPartyMark   = 0xFFFFFFFF;
static constexpr uint32_t        s_kPartyEdge   = 0xFF000000;
static constexpr uint32_t        s_kHatchDark   = 0xFF282828;
static constexpr uint32_t        s_kHatchLight  = 0xFFC8C8C8;
static constexpr int             s_kTilePixels  = 5;

static constexpr const wchar_t * s_kpszArmor[] =
{
    L"Skin", L"Cloth", L"Leather", L"Chain", L"Plate", L"+2 chain", L"+2 plate", L"Exotic",
};

static constexpr const wchar_t * s_kpszWeapons[] =
{
    L"Hands", L"Dagger", L"Mace", L"Sling", L"Axe", L"Bow", L"Sword", L"2-handed sword",
    L"+2 axe", L"+2 bow", L"+2 sword", L"Gloves", L"+4 axe", L"+4 bow", L"+4 sword", L"Exotic",
};





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::IsBcd
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima3Save::IsBcd (Byte value)
{
    constexpr Byte  kDigitMax = 9;
    constexpr int   kNibble   = 4;
    constexpr Byte  kLowMask  = 0x0F;



    return (value >> kNibble) <= kDigitMax && (value & kLowMask) <= kDigitMax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::ReadBcd
//
//  `count` bytes of binary-coded decimal at `at`, the most significant
//  first: 01 23 as 123.
//
////////////////////////////////////////////////////////////////////////////////

int Ultima3Save::ReadBcd (std::span<const Byte> bytes, size_t at, size_t count)
{
    constexpr int   kNibble   = 4;
    constexpr int   kTen      = 10;
    constexpr int   kHundred  = 100;
    constexpr Byte  kLowMask  = 0x0F;
    int             value     = 0;



    for (size_t i = 0; i < count; i++)
    {
        Byte  b = bytes[at + i];

        value = value * kHundred + (b >> kNibble) * kTen + (b & kLowMask);
    }

    return value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::TryReadCharacter
//
//  One 64-byte character, or false when the slot is empty: its name isn't
//  high-bit ASCII letters, or a stat isn't decimal.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima3Save::TryReadCharacter (std::span<const Byte> record, Character & outCharacter)
{
    constexpr size_t  kStatusAt     = 0x11;
    constexpr size_t  kStrengthAt   = 0x12;
    constexpr size_t  kDexterityAt  = 0x13;
    constexpr size_t  kIntellectAt  = 0x14;
    constexpr size_t  kWisdomAt     = 0x15;
    constexpr size_t  kRaceAt       = 0x16;
    constexpr size_t  kClassAt      = 0x17;
    constexpr size_t  kSexAt        = 0x18;
    constexpr size_t  kMagicAt      = 0x19;
    constexpr size_t  kHitPointsAt  = 0x1A;
    constexpr size_t  kMaxHitsAt    = 0x1C;
    constexpr size_t  kExperienceAt = 0x1E;
    constexpr size_t  kFoodAt       = 0x20;
    constexpr size_t  kGoldAt       = 0x23;
    constexpr size_t  kArmorAt      = 0x28;
    constexpr size_t  kWeaponAt     = 0x30;
    constexpr size_t  kWordBytes    = 2;
    constexpr Byte    kHighBit      = 0x80;
    constexpr Byte    kFirstChar    = 0xA0;
    bool              isValid       = record.size() >= kRecordBytes && record[0] >= kFirstChar;



    for (size_t i = 0; isValid && i < kNameBytes && record[i] != 0; i++)
    {
        isValid = record[i] >= kFirstChar;
    }

    for (size_t at = kStrengthAt; isValid && at <= kWisdomAt; at++)
    {
        isValid = IsBcd (record[at]);
    }

    if (!isValid)
    {
        return false;
    }

    outCharacter = Character();

    for (size_t i = 0; i < kNameBytes && record[i] != 0; i++)
    {
        outCharacter.name += (wchar_t) (record[i] & ~kHighBit);
    }

    outCharacter.status         = (wchar_t) (record[kStatusAt] & ~kHighBit);
    outCharacter.race           = (wchar_t) (record[kRaceAt]   & ~kHighBit);
    outCharacter.characterClass = (wchar_t) (record[kClassAt]  & ~kHighBit);
    outCharacter.sex            = (wchar_t) (record[kSexAt]    & ~kHighBit);
    outCharacter.strength       = ReadBcd (record, kStrengthAt,   1);
    outCharacter.dexterity      = ReadBcd (record, kDexterityAt,  1);
    outCharacter.intelligence   = ReadBcd (record, kIntellectAt,  1);
    outCharacter.wisdom         = ReadBcd (record, kWisdomAt,     1);
    outCharacter.magic          = ReadBcd (record, kMagicAt,      1);
    outCharacter.hitPoints      = ReadBcd (record, kHitPointsAt,  kWordBytes);
    outCharacter.maxHitPoints   = ReadBcd (record, kMaxHitsAt,    kWordBytes);
    outCharacter.experience     = ReadBcd (record, kExperienceAt, kWordBytes);
    outCharacter.food           = ReadBcd (record, kFoodAt,       kWordBytes);
    outCharacter.gold           = ReadBcd (record, kGoldAt,       kWordBytes);
    outCharacter.armor          = record[kArmorAt];
    outCharacter.weapon         = record[kWeaponAt];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::TryReadParty
//
//  The party, or false when none is formed: its size isn't 1 to 4, it's
//  off the map, or a member's roster slot isn't 1 to 20.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima3Save::TryReadParty (std::span<const Byte> sectors, Party & outParty)
{
    constexpr size_t  kSizeAt  = 1;
    constexpr size_t  kXAt     = 3;
    constexpr size_t  kYAt     = 4;
    constexpr size_t  kSlotsAt = 6;
    bool              isValid  = sectors.size() == kImageBytes;



    if (!isValid)
    {
        return false;
    }

    outParty      = Party();
    outParty.size = sectors[kPartyAt + kSizeAt];
    outParty.x    = sectors[kPartyAt + kXAt];
    outParty.y    = sectors[kPartyAt + kYAt];
    isValid       = outParty.size >= 1 && outParty.size <= kMaxParty && outParty.x < kMapSide && outParty.y < kMapSide;

    for (int i = 0; isValid && i < outParty.size; i++)
    {
        int  slot = sectors[kPartyAt + kSlotsAt + (size_t) i];

        isValid = slot >= 1 && slot <= kRosterSlots;
        outParty.slots.push_back (slot - 1);
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::IsFound
//
//  A party is formed, each member's character can be read, and nearly all
//  of the map is terrain: the rest is monsters and ships drawn on it.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima3Save::IsFound (std::span<const Byte> sectors)
{
    constexpr int   kTerrainPercent = 90;
    constexpr int   kPercent        = 100;
    constexpr Byte  kTileStep       = 4;
    Party           party;
    Character       character;
    int             terrain         = 0;
    bool            isFound         = TryReadParty (sectors, party);



    for (int i = 0; isFound && i < party.size; i++)
    {
        isFound = TryReadCharacter (sectors.subspan (kMembersAt + (size_t) i * kRecordBytes, kRecordBytes), character);
    }

    if (!isFound)
    {
        return false;
    }

    for (size_t i = 0; i < kMapBytes; i++)
    {
        Byte  tile = sectors[kMapAt + i];

        if (tile % kTileStep == 0 && tile / kTileStep < (Byte) std::size (s_kTerrain))
        {
            terrain++;
        }
    }

    return terrain * kPercent >= (int) kMapBytes * kTerrainPercent;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::RenderMap
//
//  Each tile s_kTilePixels square in its terrain's color, the party's tile
//  a white square with a black edge, and the rows a sector that couldn't
//  be read covers in dark and light diagonal stripes.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima3Save::RenderMap (std::span<const Byte> sectors, std::span<const bool> unreadable, int partyX, int partyY, std::vector<uint32_t> & outBgra, int & outWidth, int & outHeight)
{
    constexpr Byte  kTileStep    = 4;
    constexpr int   kStripeWidth = 2;
    int             side         = kMapSide * s_kTilePixels;
    size_t          rowBytes     = (size_t) kMapSide;



    outWidth  = side;
    outHeight = side;
    outBgra.assign ((size_t) side * (size_t) side, s_kOtherTile);

    for (int y = 0; y < kMapSide; y++)
    {
        size_t  rowAt   = kMapAt + (size_t) y * rowBytes;
        size_t  sector  = rowAt / kSectorBytes;
        bool    isLost  = sector < unreadable.size() && unreadable[sector];

        for (int x = 0; x < kMapSide; x++)
        {
            Byte      tile    = sectors[rowAt + (size_t) x];
            size_t    terrain = tile / kTileStep;
            bool      isParty = x == partyX && y == partyY;
            uint32_t  argb    = (tile % kTileStep == 0 && terrain < std::size (s_kTerrain)) ? s_kTerrain[terrain].argb : s_kOtherTile;

            for (int py = 0; py < s_kTilePixels; py++)
            {
                for (int px = 0; px < s_kTilePixels; px++)
                {
                    int       column = x * s_kTilePixels + px;
                    int       row    = y * s_kTilePixels + py;
                    bool      isEdge = px == 0 || py == 0 || px == s_kTilePixels - 1 || py == s_kTilePixels - 1;
                    uint32_t  pixel  = argb;

                    if (isLost)
                    {
                        pixel = ((column + row) / kStripeWidth) % 2 == 0 ? s_kHatchDark : s_kHatchLight;
                    }

                    if (isParty)
                    {
                        pixel = isEdge ? s_kPartyEdge : s_kPartyMark;
                    }

                    outBgra[(size_t) row * (size_t) side + (size_t) column] = pixel;
                }
            }
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::GetRaceName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima3Save::GetRaceName (wchar_t letter)
{
    switch (letter)
    {
        case L'H': return L"Human";
        case L'E': return L"Elf";
        case L'D': return L"Dwarf";
        case L'B': return L"Bobbit";
        case L'F': return L"Fuzzy";
        default:   return std::wstring (1, letter);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::GetClassName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima3Save::GetClassName (wchar_t letter)
{
    switch (letter)
    {
        case L'F': return L"fighter";
        case L'C': return L"cleric";
        case L'W': return L"wizard";
        case L'T': return L"thief";
        case L'P': return L"paladin";
        case L'B': return L"barbarian";
        case L'L': return L"lark";
        case L'I': return L"illusionist";
        case L'D': return L"druid";
        case L'A': return L"alchemist";
        case L'R': return L"ranger";
        default:   return std::wstring (1, letter);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::GetStatusName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima3Save::GetStatusName (wchar_t letter)
{
    switch (letter)
    {
        case L'G': return L"Good";
        case L'P': return L"Poisoned";
        case L'D': return L"Dead";
        case L'A': return L"Ashes";
        default:   return std::wstring (1, letter);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::GetArmorName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima3Save::GetArmorName (int armor)
{
    bool  isKnown = armor >= 0 && armor < (int) std::size (s_kpszArmor);



    return isKnown ? std::wstring (s_kpszArmor[armor]) : std::format (L"Armor {}", armor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::GetWeaponName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima3Save::GetWeaponName (int weapon)
{
    bool  isKnown = weapon >= 0 && weapon < (int) std::size (s_kpszWeapons);



    return isKnown ? std::wstring (s_kpszWeapons[weapon]) : std::format (L"Weapon {}", weapon);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::MakeParagraph
//
////////////////////////////////////////////////////////////////////////////////

Paragraph Ultima3Save::MakeParagraph (std::vector<Run> runs)
{
    Paragraph  paragraph;



    paragraph.runs = std::move (runs);

    return paragraph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::AddCharacter
//
//  One character as a short block, one fact to a line, which reads in a
//  narrow pane: the name in bold and the race, class and sex, then the
//  status, in red unless good, the stats, hit and magic points,
//  experience, food, gold, armor and weapon.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima3Save::AddCharacter (const Character & character, std::vector<Paragraph> & ioParagraphs)
{
    std::wstring  sex     = (character.sex == L'M') ? L"male" : (character.sex == L'F') ? L"female" : L"other";
    bool          isGood  = character.status == L'G';



    ioParagraphs.push_back (Paragraph());
    ioParagraphs.push_back (MakeParagraph ({ Run { character.name, true },
                                             Run { std::format (L"   {} {}, {}", GetRaceName (character.race), GetClassName (character.characterClass), sex),
                                                   false, false, false, RunColor::Muted } }));
    ioParagraphs.push_back (MakeParagraph ({ Run { GetStatusName (character.status), false, false, false, isGood ? RunColor::Text : RunColor::Bad },
                                             Run { std::format (L"   HP {} / {}   MP {}", character.hitPoints, character.maxHitPoints, character.magic) } }));
    ioParagraphs.push_back (MakeParagraph ({ Run { std::format (L"Str {}   Dex {}   Int {}   Wis {}",
                                                                character.strength, character.dexterity, character.intelligence, character.wisdom) } }));
    ioParagraphs.push_back (MakeParagraph ({ Run { std::format (L"Exp {}   Food {}   Gold {}", character.experience, character.food, character.gold) } }));
    ioParagraphs.push_back (MakeParagraph ({ Run { GetArmorName (character.armor) + L", " + GetWeaponName (character.weapon) } }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::AddLegend
//
//  What the map's colors indicate, as one paragraph that wraps to the
//  pane. Hovering over a tile shows its terrain, so the legend is short.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima3Save::AddLegend (std::vector<Paragraph> & ioParagraphs)
{
    std::wstring  text = L"Party: white square   ";



    for (const auto & terrain : s_kTerrain)
    {
        text += std::format (L"{}: {}   ", terrain.pszColor, terrain.pszName);
    }

    text += L"Pink: monster or ship   Stripes: couldn't be read";

    ioParagraphs.push_back (MakeParagraph ({ Run { text, false, false, false, RunColor::Muted } }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::LabelMap
//
//  A tip for each of the map's tiles: its column and row, then what is
//  there with the color the map draws it in. The party's tile has the
//  party and the terrain under it, and a row an unreadable sector covers
//  says so.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima3Save::LabelMap (std::span<const Byte> sectors, std::span<const bool> unreadable, int partyX, int partyY, Paragraph & ioMap)
{
    constexpr Byte  kTileStep = 4;



    ioMap.tipCell    = s_kTilePixels;
    ioMap.tipLeft    = 0;
    ioMap.tipTop     = 0;
    ioMap.tipColumns = kMapSide;
    ioMap.tips.assign ((size_t) kMapSide * (size_t) kMapSide, PictureTip());

    for (int y = 0; y < kMapSide; y++)
    {
        size_t  rowAt   = kMapAt + (size_t) y * (size_t) kMapSide;
        size_t  sector  = rowAt / kSectorBytes;
        bool    isLost  = sector < unreadable.size() && unreadable[sector];

        for (int x = 0; x < kMapSide; x++)
        {
            Byte          tile      = sectors[rowAt + (size_t) x];
            size_t        terrain   = tile / kTileStep;
            bool          isTerrain = tile % kTileStep == 0 && terrain < std::size (s_kTerrain);
            PictureTip  & tip       = ioMap.tips[(size_t) y * (size_t) kMapSide + (size_t) x];
            std::wstring  name      = isTerrain ? std::wstring (s_kTerrain[terrain].pszName) : std::wstring (L"monster or ship");

            name[0] = (wchar_t) towupper (name[0]);

            tip.lines.push_back ({ 0, std::format (L"Column {}, row {}", x, y) });

            if (isLost)
            {
                tip.lines.push_back ({ s_kHatchDark, L"Couldn't be read" });
                continue;
            }

            if (x == partyX && y == partyY)
            {
                tip.lines.push_back ({ s_kPartyMark, L"The party" });
            }

            tip.lines.push_back ({ isTerrain ? s_kTerrain[terrain].argb : s_kOtherTile, name });
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  Ultima3Save::Decode
//
//  The title, the map with the party marked and its legend, the members
//  in party order, and the other characters on the roster. A member's
//  roster slot has the character as it was when the party formed, so it
//  isn't listed again.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ultima3Save::Decode (std::span<const Byte> sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT    hr       = S_OK;
    Party      party;
    Character  character;
    Paragraph  map;
    bool       isFound  = IsFound (sectors);
    bool       isParty  = false;



    outFile        = DecodedFile();
    outFile.format = FileFormat::UltimaSave;
    outFile.kind   = DecodedFile::Kind::Document;

    CBRFEx (isFound, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The disk doesn't have an Ultima III saved game.");

    isParty = TryReadParty (sectors, party);
    CBRAEx (isParty, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    RenderMap (sectors, unreadable, party.x, party.y, map.picture, map.width, map.height);
    LabelMap  (sectors, unreadable, party.x, party.y, map);

    outFile.paragraphs.push_back (MakeParagraph ({ Run { L"Ultima III: Exodus", true } }));
    outFile.paragraphs.push_back (MakeParagraph ({ Run { std::format (L"Saved game, the party at column {}, row {}", party.x, party.y), false, false, false, RunColor::Muted } }));
    outFile.paragraphs.push_back (std::move (map));
    AddLegend (outFile.paragraphs);

    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (MakeParagraph ({ Run { L"Party", true } }));

    for (int i = 0; i < party.size; i++)
    {
        if (TryReadCharacter (sectors.subspan (kMembersAt + (size_t) i * kRecordBytes, kRecordBytes), character))
        {
            AddCharacter (character, outFile.paragraphs);
        }
    }

    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (MakeParagraph ({ Run { L"Roster", true } }));

    for (int slot = 0; slot < kRosterSlots; slot++)
    {
        bool  isMember = std::find (party.slots.begin(), party.slots.end(), slot) != party.slots.end();

        if (!isMember && TryReadCharacter (sectors.subspan (kRosterAt + (size_t) slot * kRecordBytes, kRecordBytes), character))
        {
            AddCharacter (character, outFile.paragraphs);
        }
    }

Error:
    return hr;
}
