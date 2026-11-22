#include "Pch.h"

#include "Machines/Apple2/Common/Ultima4Save.h"
#include "Machines/Apple2/Common/Dos33Volume.h"





//  The world's tiles by number, each with the color the map draws it in
//  and its name in a tip. A number not in the table is drawn in
//  s_kOtherTile and named by its number.
static constexpr struct { Byte tile; uint32_t argb; const wchar_t * pszName; } s_kTiles[] =
{
    { 0x00, 0xFF141E78, L"Deep water"            },
    { 0x01, 0xFF2846AA, L"Water"                 },
    { 0x02, 0xFF5078D2, L"Shallow water"         },
    { 0x03, 0xFF50643C, L"Swamp"                 },
    { 0x04, 0xFF5AAA46, L"Grass"                 },
    { 0x05, 0xFF96AA50, L"Scrub"                 },
    { 0x06, 0xFF1E692D, L"Forest"                },
    { 0x07, 0xFF96825A, L"Hills"                 },
    { 0x08, 0xFF7D7369, L"Mountains"             },
    { 0x09, 0xFF3C2819, L"Dungeon entrance"      },
    { 0x0A, 0xFFDCBE3C, L"Town"                  },
    { 0x0B, 0xFFA8A8B4, L"Castle"                },
    { 0x0C, 0xFFE6A03C, L"Village"               },
    { 0x0D, 0xFFA8A8B4, L"Lord British's castle" },
    { 0x0E, 0xFFA8A8B4, L"Lord British's castle" },
    { 0x0F, 0xFFA8A8B4, L"Lord British's castle" },
    { 0x17, 0xFFA0785A, L"Bridge"                },
    { 0x1E, 0xFFC8C8FF, L"Shrine"                },
    { 0x4C, 0xFFE63C1E, L"Lava"                  },
};

static constexpr uint32_t        s_kOtherTile   = 0xFFC850C8;
static constexpr uint32_t        s_kPartyMark   = 0xFFFFFFFF;
static constexpr uint32_t        s_kPartyEdge   = 0xFF000000;
static constexpr uint32_t        s_kHatchDark   = 0xFF282828;
static constexpr uint32_t        s_kHatchLight  = 0xFFC8C8C8;
static constexpr int             s_kTilePixels  = 2;

static constexpr const wchar_t * s_kpszClasses[] =
{
    L"Mage", L"Bard", L"Fighter", L"Druid", L"Tinker", L"Paladin", L"Ranger", L"Shepherd",
};

static constexpr const wchar_t * s_kpszWeapons[] =
{
    L"Hands", L"Staff", L"Dagger", L"Sling", L"Mace", L"Axe", L"Sword", L"Bow",
    L"Crossbow", L"Flaming oil", L"Halberd", L"Magic axe", L"Magic sword", L"Magic bow", L"Magic wand", L"Mystic sword",
};

static constexpr const wchar_t * s_kpszArmor[] =
{
    L"Skin", L"Cloth", L"Leather", L"Chain mail", L"Plate mail", L"Magic chain", L"Magic plate", L"Mystic robe",
};

static constexpr const wchar_t * s_kpszVirtues[] =
{
    L"Honesty", L"Compassion", L"Valor", L"Justice", L"Sacrifice", L"Honor", L"Spirituality", L"Humility",
};





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::IsBcd
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima4Save::IsBcd (Byte value)
{
    constexpr Byte  kDigitMax = 9;
    constexpr int   kNibble   = 4;
    constexpr Byte  kLowMask  = 0x0F;



    return (value >> kNibble) <= kDigitMax && (value & kLowMask) <= kDigitMax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::ReadBcd
//
//  `count` bytes of binary-coded decimal at `at`, the most significant
//  first: 01 23 as 123.
//
////////////////////////////////////////////////////////////////////////////////

int Ultima4Save::ReadBcd (std::span<const Byte> bytes, size_t at, size_t count)
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
//  Ultima4Save::GetTile
//
//  Region (x / 16, y / 16) is sector y / 16 * 16 + x / 16 of the disk, its
//  tiles a row of 16 at a time.
//
////////////////////////////////////////////////////////////////////////////////

Byte Ultima4Save::GetTile (std::span<const Byte> sectors, int x, int y)
{
    size_t  region = (size_t) (y / kRegionSide) * kRegionSide + (size_t) (x / kRegionSide);



    return sectors[region * kSectorBytes + (size_t) (y % kRegionSide) * kRegionSide + (size_t) (x % kRegionSide)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::TryReadFiles
//
//  ROST and PRTY from the disk's catalog, each its expected size, or false.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima4Save::TryReadFiles (const std::vector<Byte> & sectors, std::vector<Byte> & outRoster, std::vector<Byte> & outParty)
{
    HRESULT      hr       = S_OK;
    Dos33Volume  volume (sectors);
    FilePayload  roster;
    FilePayload  party;
    bool         isSized  = false;



    hr = volume.Read (FilePath::Parse ("ROST"), roster);
    CHR (hr);

    hr = volume.Read (FilePath::Parse ("PRTY"), party);
    CHR (hr);

    isSized = roster.bytes.size() >= kRosterBytes && party.bytes.size() >= kPartyBytes;
    CBR (isSized);

    outRoster = std::move (roster.bytes);
    outParty  = std::move (party.bytes);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::TryReadCharacter
//
//  One 32-byte character, or false when the slot is empty: its name isn't
//  high-bit ASCII letters, or a stat isn't decimal.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima4Save::TryReadCharacter (std::span<const Byte> record, Character & outCharacter)
{
    constexpr size_t  kSexAt        = 0x10;
    constexpr size_t  kClassAt      = 0x11;
    constexpr size_t  kStatusAt     = 0x12;
    constexpr size_t  kStrengthAt   = 0x13;
    constexpr size_t  kDexterityAt  = 0x14;
    constexpr size_t  kIntellectAt  = 0x15;
    constexpr size_t  kMagicAt      = 0x16;
    constexpr size_t  kHitPointsAt  = 0x18;
    constexpr size_t  kMaxHitsAt    = 0x1A;
    constexpr size_t  kExperienceAt = 0x1C;
    constexpr size_t  kWeaponAt     = 0x1E;
    constexpr size_t  kArmorAt      = 0x1F;
    constexpr size_t  kWordBytes    = 2;
    constexpr Byte    kFemaleGlyph  = 0x7B;
    constexpr Byte    kHighBit      = 0x80;
    constexpr Byte    kFirstChar    = 0xA0;
    bool              isValid       = record.size() >= kRecordBytes && record[0] >= kFirstChar;



    for (size_t i = 0; isValid && i < kNameBytes && record[i] != 0; i++)
    {
        isValid = record[i] >= kFirstChar;
    }

    for (size_t at = kStrengthAt; isValid && at <= kMagicAt; at++)
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

    outCharacter.isFemale     = record[kSexAt] == kFemaleGlyph;
    outCharacter.classIndex   = record[kClassAt];
    outCharacter.status       = (wchar_t) (record[kStatusAt] & ~kHighBit);
    outCharacter.strength     = ReadBcd (record, kStrengthAt,   1);
    outCharacter.dexterity    = ReadBcd (record, kDexterityAt,  1);
    outCharacter.intelligence = ReadBcd (record, kIntellectAt,  1);
    outCharacter.magic        = ReadBcd (record, kMagicAt,      1);
    outCharacter.hitPoints    = ReadBcd (record, kHitPointsAt,  kWordBytes);
    outCharacter.maxHitPoints = ReadBcd (record, kMaxHitsAt,    kWordBytes);
    outCharacter.experience   = ReadBcd (record, kExperienceAt, kWordBytes);
    outCharacter.weapon       = record[kWeaponAt];
    outCharacter.armor        = record[kArmorAt];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::IsFound
//
//  Nearly all of tracks 0 to 15 is terrain, ROST and PRTY are in the
//  catalog, and the first character can be read. The terrain is checked
//  first: it rejects other DOS 3.3 disks, whose first tracks are DOS and
//  files, before the catalog is read.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima4Save::IsFound (const std::vector<Byte> & sectors)
{
    constexpr int      kTerrainPercent = 90;
    constexpr int      kPercent        = 100;
    constexpr Byte     kLastTerrain    = 0x0F;
    constexpr int      kWorldTiles     = kWorldSide * kWorldSide;
    std::vector<Byte>  roster;
    std::vector<Byte>  party;
    Character          character;
    int                terrain         = 0;



    if (sectors.size() != kImageBytes)
    {
        return false;
    }

    for (int i = 0; i < kWorldTiles; i++)
    {
        if (sectors[(size_t) i] <= kLastTerrain)
        {
            terrain++;
        }
    }

    return terrain * kPercent >= kWorldTiles * kTerrainPercent
        && TryReadFiles (sectors, roster, party)
        && TryReadCharacter (std::span<const Byte> (roster).subspan (0, kRecordBytes), character);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::GetTileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima4Save::GetTileName (Byte tile)
{
    for (const auto & known : s_kTiles)
    {
        if (known.tile == tile)
        {
            return known.pszName;
        }
    }

    return std::format (L"Tile {:02X}", tile);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::GetTileColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Ultima4Save::GetTileColor (Byte tile)
{
    for (const auto & known : s_kTiles)
    {
        if (known.tile == tile)
        {
            return known.argb;
        }
    }

    return s_kOtherTile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::RenderMap
//
//  Each tile s_kTilePixels square in its color, with a tip of its column
//  and row and its name. The party is a white square with a black edge,
//  three tiles wide so it shows at this scale, and the tiles a sector that
//  couldn't be read covers are striped.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima4Save::RenderMap (std::span<const Byte> sectors, std::span<const bool> unreadable, int partyX, int partyY, Paragraph & ioMap)
{
    constexpr int  kMarkTiles   = 3;
    constexpr int  kStripeWidth = 2;
    int            side         = kWorldSide * s_kTilePixels;
    int            markLeft     = (partyX - kMarkTiles / 2) * s_kTilePixels;
    int            markTop      = (partyY - kMarkTiles / 2) * s_kTilePixels;
    int            markSide     = kMarkTiles * s_kTilePixels;



    ioMap.width      = side;
    ioMap.height     = side;
    ioMap.tipCell    = s_kTilePixels;
    ioMap.tipLeft    = 0;
    ioMap.tipTop     = 0;
    ioMap.tipColumns = kWorldSide;
    ioMap.picture.assign ((size_t) side * (size_t) side, s_kOtherTile);
    ioMap.tips.assign ((size_t) kWorldSide * (size_t) kWorldSide, PictureTip());

    for (int y = 0; y < kWorldSide; y++)
    {
        for (int x = 0; x < kWorldSide; x++)
        {
            Byte          tile    = GetTile (sectors, x, y);
            size_t        sector  = (size_t) (y / kRegionSide) * kRegionSide + (size_t) (x / kRegionSide);
            bool          isLost  = sector < unreadable.size() && unreadable[sector];
            PictureTip  & tip     = ioMap.tips[(size_t) y * kWorldSide + (size_t) x];

            for (int py = 0; py < s_kTilePixels; py++)
            {
                for (int px = 0; px < s_kTilePixels; px++)
                {
                    int  column = x * s_kTilePixels + px;
                    int  row    = y * s_kTilePixels + py;

                    ioMap.picture[(size_t) row * (size_t) side + (size_t) column] = isLost ? (((column + row) / kStripeWidth) % 2 == 0 ? s_kHatchDark : s_kHatchLight)
                                                                                           : GetTileColor (tile);
                }
            }

            tip.lines.push_back ({ 0, std::format (L"Column {}, row {}", x, y) });

            if (isLost)
            {
                tip.lines.push_back ({ kHatchedSwatch, L"Couldn't be read" });
                continue;
            }

            if (x == partyX && y == partyY)
            {
                tip.lines.push_back ({ s_kPartyMark, L"The party" });
            }

            tip.lines.push_back ({ GetTileColor (tile), GetTileName (tile) });
        }
    }

    for (int row = (std::max) (0, markTop); row < (std::min) (side, markTop + markSide); row++)
    {
        for (int column = (std::max) (0, markLeft); column < (std::min) (side, markLeft + markSide); column++)
        {
            bool  isEdge = row == markTop || column == markLeft || row == markTop + markSide - 1 || column == markLeft + markSide - 1;

            ioMap.picture[(size_t) row * (size_t) side + (size_t) column] = isEdge ? s_kPartyEdge : s_kPartyMark;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::GetClassName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima4Save::GetClassName (int classIndex)
{
    bool  isKnown = classIndex >= 0 && classIndex < (int) std::size (s_kpszClasses);



    return isKnown ? std::wstring (s_kpszClasses[classIndex]) : std::format (L"Class {}", classIndex);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::GetStatusName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima4Save::GetStatusName (wchar_t letter)
{
    switch (letter)
    {
        case L'G': return L"Good";
        case L'P': return L"Poisoned";
        case L'S': return L"Asleep";
        case L'D': return L"Dead";
        default:   return std::wstring (1, letter);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::GetWeaponName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima4Save::GetWeaponName (int weapon)
{
    bool  isKnown = weapon >= 0 && weapon < (int) std::size (s_kpszWeapons);



    return isKnown ? std::wstring (s_kpszWeapons[weapon]) : std::format (L"Weapon {}", weapon);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::GetArmorName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima4Save::GetArmorName (int armor)
{
    bool  isKnown = armor >= 0 && armor < (int) std::size (s_kpszArmor);



    return isKnown ? std::wstring (s_kpszArmor[armor]) : std::format (L"Armor {}", armor);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::MakeParagraph
//
////////////////////////////////////////////////////////////////////////////////

Paragraph Ultima4Save::MakeParagraph (std::vector<Run> runs)
{
    Paragraph  paragraph;



    paragraph.runs = std::move (runs);

    return paragraph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::MakeLabel
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima4Save::MakeLabel (const std::wstring & text)
{
    return Run { text, false, false, false, RunColor::Muted };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::MakeValue
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima4Save::MakeValue (const std::wstring & text)
{
    return Run { text, true, false, false, RunColor::Keyword };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::GetHealthColor
//
//  Green at full hit points, red at a quarter or less, and the text color
//  between.
//
////////////////////////////////////////////////////////////////////////////////

RunColor Ultima4Save::GetHealthColor (int hitPoints, int maxHitPoints)
{
    constexpr int  kLowQuarters = 4;



    return (maxHitPoints > 0 && hitPoints >= maxHitPoints)          ? RunColor::Good
         : (hitPoints * kLowQuarters <= maxHitPoints)               ? RunColor::Bad
                                                                    : RunColor::Text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::AddCharacter
//
//  A character as a table row, and as a card of five short lines for a
//  narrow pane: the name and the class and sex, the status, hit and magic
//  points, the stats, the experience and level, and the armor and weapon.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima4Save::AddCharacter (const Character & character, RecordTable & ioTable)
{
    constexpr int        kHitsPerLevel = 100;
    std::wstring         sex           = character.isFemale ? L"female" : L"male";
    RunColor             status        = (character.status == L'G') ? RunColor::Good : RunColor::Bad;
    RunColor             health        = GetHealthColor (character.hitPoints, character.maxHitPoints);
    RunColor             nameColor     = (character.status == L'D') ? RunColor::Bad : RunColor::Text;
    int                  level         = character.maxHitPoints / kHitsPerLevel;
    RecordTable::Record  record;



    record.cells = { Run { character.name, true, false, false, nameColor },
                     Run { GetClassName (character.classIndex) },
                     Run { sex },
                     Run { GetStatusName (character.status), true, false, false, status },
                     Run { std::format (L"{} / {} / {}", character.strength, character.dexterity, character.intelligence) },
                     Run { std::format (L"{} / {}", character.hitPoints, character.maxHitPoints), true, false, false, health },
                     Run { std::format (L"{}", character.magic) },
                     Run { std::format (L"{}", level) },
                     Run { std::format (L"{}", character.experience) },
                     Run { GetArmorName (character.armor) },
                     Run { GetWeaponName (character.weapon) } };

    record.card.push_back (MakeParagraph ({ Run { character.name, true, false, false, nameColor },
                                            Run { std::format (L"   {}, {}", GetClassName (character.classIndex), sex), false, true, false, RunColor::Muted } }));
    record.card.push_back (MakeParagraph ({ Run { GetStatusName (character.status), true, false, false, status },
                                            MakeLabel (L"   HP "), Run { std::format (L"{} / {}", character.hitPoints, character.maxHitPoints), true, false, false, health },
                                            MakeLabel (L"   MP "), MakeValue (std::format (L"{}", character.magic)) }));
    record.card.push_back (MakeParagraph ({ MakeLabel (L"Str "),    MakeValue (std::format (L"{}", character.strength)),
                                            MakeLabel (L"   Dex "), MakeValue (std::format (L"{}", character.dexterity)),
                                            MakeLabel (L"   Int "), MakeValue (std::format (L"{}", character.intelligence)) }));
    record.card.push_back (MakeParagraph ({ MakeLabel (L"Level "),  MakeValue (std::format (L"{}", level)),
                                            MakeLabel (L"   Exp "), MakeValue (std::format (L"{}", character.experience)) }));
    record.card.push_back (MakeParagraph ({ MakeLabel (L"Armor "),     Run { GetArmorName (character.armor) },
                                            MakeLabel (L"   Weapon "), Run { GetWeaponName (character.weapon) } }));
    ioTable.records.push_back (std::move (record));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::AddLegend
//
//  A color sample and name for the party and each kind of place on the
//  world, as one paragraph that wraps to the pane. Hovering over a tile
//  shows its name, so the legend is short.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima4Save::AddLegend (std::vector<Paragraph> & ioParagraphs)
{
    static constexpr Byte  kShown[] = { 0x01, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x17, 0x1E, 0x4C };
    std::vector<Run>       runs;



    runs.push_back (Run { L"Party   ", false, false, false, RunColor::Muted, s_kPartyMark });

    for (Byte tile : kShown)
    {
        runs.push_back (Run { GetTileName (tile) + L"   ", false, false, false, RunColor::Muted, GetTileColor (tile) });
    }

    runs.push_back (Run { L"Other   ",         false, false, false, RunColor::Muted, s_kOtherTile });
    runs.push_back (Run { L"Couldn't be read", false, false, false, RunColor::Muted, kHatchedSwatch });

    ioParagraphs.push_back (MakeParagraph (std::move (runs)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::AddVirtues
//
//  The eight virtues as a one-row table, which reflows like the
//  characters.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima4Save::AddVirtues (std::span<const Byte> roster, std::vector<Paragraph> & ioParagraphs)
{
    auto                 table = std::make_shared<RecordTable>();
    RecordTable::Record  record;
    std::wstring         line;
    Paragraph            paragraph;



    for (int i = 0; i < kVirtues; i++)
    {
        int  value = ReadBcd (roster, kVirtuesAt + (size_t) i, 1);

        table->headings.push_back (s_kpszVirtues[i]);
        record.cells.push_back (Run { std::format (L"{}", value) });
        line += std::format (L"{} {}   ", s_kpszVirtues[i], value);
    }

    record.card.push_back (MakeParagraph ({ Run { line } }));
    table->title = L"Virtues";
    table->records.push_back (std::move (record));
    paragraph.table = table;

    ioParagraphs.push_back (Paragraph());
    ioParagraphs.push_back (std::move (paragraph));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima4Save::Decode
//
//  The title, the world with the party marked, a short legend, the
//  characters, and the virtues.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ultima4Save::Decode (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError)
{
    constexpr size_t   kXAt      = 0;
    constexpr size_t   kYAt      = 1;
    HRESULT            hr        = S_OK;
    std::vector<Byte>  roster;
    std::vector<Byte>  party;
    Character          character;
    Paragraph          map;
    Paragraph          characters;
    auto               table     = std::make_shared<RecordTable>();
    bool               isFound   = IsFound (sectors);
    bool               hasFiles  = false;
    int                x         = 0;
    int                y         = 0;



    outFile        = DecodedFile();
    outFile.format = FileFormat::UltimaSave;
    outFile.kind   = DecodedFile::Kind::Document;

    CBRFEx (isFound, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The disk doesn't have an Ultima IV saved game.");

    hasFiles = TryReadFiles (sectors, roster, party);
    CBRAEx (hasFiles, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    x = party[kXAt];
    y = party[kYAt];

    RenderMap (sectors, unreadable, x, y, map);

    outFile.paragraphs.push_back (MakeParagraph ({ Run { L"Ultima IV: Quest of the Avatar", true } }));
    outFile.paragraphs.push_back (MakeParagraph ({ Run { std::format (L"Saved game, the party at column {}, row {}", x, y), false, false, false, RunColor::Muted } }));
    outFile.paragraphs.push_back (std::move (map));
    AddLegend (outFile.paragraphs);

    table->headings = { L"Name", L"Class", L"Sex", L"Status", L"Str / Dex / Int", L"HP", L"MP", L"Level", L"Exp", L"Armor", L"Weapon" };

    for (int i = 0; i < kCharacters; i++)
    {
        if (TryReadCharacter (std::span<const Byte> (roster).subspan ((size_t) i * kRecordBytes, kRecordBytes), character))
        {
            AddCharacter (character, *table);
        }
    }

    table->title     = L"Characters";
    characters.table = table;

    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (std::move (characters));

    AddVirtues (roster, outFile.paragraphs);

Error:
    return hr;
}
