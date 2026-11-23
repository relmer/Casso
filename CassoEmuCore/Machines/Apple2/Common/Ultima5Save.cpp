#include "Pch.h"

#include "Machines/Apple2/Common/Ultima5Save.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"





//  The world's tiles by number, each with its color on the map and its name
//  in a tip; the first range that matches wins. Water, grass, forest and
//  towns were checked in the game against the party's surroundings. The
//  rest are identified by where they lie on the map: eight shrines, roads
//  that link the towns, and castles at Lord British's and Blackthorn's
//  coordinates. A number not in the table is drawn in s_kOtherTile.
static constexpr struct { Byte first; Byte last; uint32_t argb; const wchar_t * pszName; } s_kTiles[] =
{
    { 0x01, 0x01, 0xFF141E78, L"Deep water"    },
    { 0x02, 0x02, 0xFF2846AA, L"Water"         },
    { 0x03, 0x03, 0xFF5078D2, L"Shallow water" },
    { 0x04, 0x04, 0xFF50643C, L"Swamp"         },
    { 0x05, 0x05, 0xFF5AAA46, L"Grass"         },
    { 0x06, 0x06, 0xFF96AA50, L"Brush"         },
    { 0x07, 0x07, 0xFF145A28, L"Deep forest"   },
    { 0x08, 0x08, 0xFF1E692D, L"Forest"        },
    { 0x09, 0x09, 0xFF96825A, L"Hills"         },
    { 0x0A, 0x0F, 0xFF7D7369, L"Mountains"     },
    { 0x14, 0x14, 0xFFB48CDC, L"Shrine"        },
    { 0x19, 0x19, 0xFFDCBE3C, L"Town"          },
    { 0x10, 0x1C, 0xFFE6D282, L"Other place"   },
    { 0x30, 0x37, 0xFF6E8C6E, L"Shore"         },
    { 0x39, 0x3F, 0xFFA8A8B4, L"Castle"        },
    { 0x47, 0x47, 0xFF8C5A32, L"Bridge"        },
    { 0x60, 0x6F, 0xFFB49664, L"Road"          },
};

static constexpr uint32_t        s_kOtherTile   = 0xFFC850C8;
static constexpr uint32_t        s_kPartyMark   = 0xFFFFFFFF;
static constexpr uint32_t        s_kPartyEdge   = 0xFF000000;
static constexpr uint32_t        s_kHatchDark   = 0xFF282828;
static constexpr uint32_t        s_kHatchLight  = 0xFFC8C8C8;
static constexpr int             s_kTilePixels  = 2;





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::IsBcd
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima5Save::IsBcd (Byte value)
{
    constexpr Byte  kDigitMax = 9;
    constexpr int   kNibble   = 4;
    constexpr Byte  kLowMask  = 0x0F;



    return (value >> kNibble) <= kDigitMax && (value & kLowMask) <= kDigitMax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::ReadBcd
//
//  `count` bytes of binary-coded decimal at `at`, the most significant
//  first: 06 60 as 660.
//
////////////////////////////////////////////////////////////////////////////////

int Ultima5Save::ReadBcd (std::span<const Byte> bytes, size_t at, size_t count)
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
//  Ultima5Save::GetTile
//
//  The world is 16 by 16 chunks of 16 by 16 tiles, a chunk a half block
//  from block kMapFirstBlock, west to east and north to south.
//
////////////////////////////////////////////////////////////////////////////////

Byte Ultima5Save::GetTile (const std::vector<Byte> & sectors, int x, int y)
{
    int     chunk  = (y / kRegionSide) * kRegionSide + (x / kRegionSide);
    size_t  offset = ProDosSkeleton::GetBlockByteOffset (sectors.size(), kMapFirstBlock + chunk / 2, (size_t) (chunk % 2) * kChunkBytes);



    return sectors[offset + (size_t) (y % kRegionSide) * kRegionSide + (size_t) (x % kRegionSide)];
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::TryReadFiles
//
//  ROSTER and PRTY.DATA from the volume, each at least its minimum size, or false.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima5Save::TryReadFiles (const std::vector<Byte> & sectors, std::vector<Byte> & outRoster, std::vector<Byte> & outParty)
{
    HRESULT       hr      = S_OK;
    ProDosVolume  volume (sectors);
    FilePayload   roster;
    FilePayload   party;
    bool          isSized = false;



    hr = volume.Read (FilePath::Parse ("ROSTER"), roster);
    CHR (hr);

    hr = volume.Read (FilePath::Parse ("PRTY.DATA"), party);
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
//  Ultima5Save::TryReadCharacter
//
//  Character `index`: its name, up to 8 high-bit letters, the last with its
//  high bit clear, and its 16-byte record: whether it's in the party, its
//  sex, class and status, Str, Dex, Int and MP, then hit points, maximum
//  hit points and experience, two bytes each, and its level, all decimal.
//  False for an empty slot.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima5Save::TryReadCharacter (std::span<const Byte> roster, int index, Character & outCharacter)
{
    constexpr Byte    kHighBit      = 0x80;
    constexpr Byte    kFirstLetter  = 0x41;
    constexpr Byte    kFemaleGlyph  = 0x3D;
    constexpr size_t  kInPartyAt    = 0;
    constexpr size_t  kSexAt        = 1;
    constexpr size_t  kClassAt      = 2;
    constexpr size_t  kStatusAt     = 3;
    constexpr size_t  kStrengthAt   = 4;
    constexpr size_t  kMagicAt      = 7;
    constexpr size_t  kHitPointsAt  = 8;
    constexpr size_t  kMaxHitsAt    = 10;
    constexpr size_t  kExperienceAt = 12;
    constexpr size_t  kLevelAt      = 14;
    constexpr size_t  kWordBytes    = 2;
    size_t            nameAt        = (size_t) index * kNameBytes;
    size_t            at            = kRecordsAt + (size_t) index * kRecordBytes;
    bool              isValid       = roster.size() >= kRosterBytes && index >= 0 && index < kCharacters
                                      && (Byte) (roster[nameAt] & ~kHighBit) >= kFirstLetter;



    for (size_t i = kStrengthAt; isValid && i <= kMagicAt; i++)
    {
        isValid = IsBcd (roster[at + i]);
    }

    if (!isValid)
    {
        return false;
    }

    outCharacter = Character();

    for (size_t i = 0; i < kNameBytes && roster[nameAt + i] != 0; i++)
    {
        outCharacter.name += (wchar_t) (roster[nameAt + i] & ~kHighBit);

        if ((roster[nameAt + i] & kHighBit) == 0)
        {
            break;
        }
    }

    outCharacter.inParty        = roster[at + kInPartyAt] == 0;
    outCharacter.isFemale       = roster[at + kSexAt] == kFemaleGlyph;
    outCharacter.characterClass = (wchar_t) (roster[at + kClassAt]  & ~kHighBit);
    outCharacter.status         = (wchar_t) (roster[at + kStatusAt] & ~kHighBit);
    outCharacter.strength       = ReadBcd (roster, at + kStrengthAt,     1);
    outCharacter.dexterity      = ReadBcd (roster, at + kStrengthAt + 1, 1);
    outCharacter.intelligence   = ReadBcd (roster, at + kStrengthAt + 2, 1);
    outCharacter.magic          = ReadBcd (roster, at + kMagicAt,        1);
    outCharacter.hitPoints      = ReadBcd (roster, at + kHitPointsAt,    kWordBytes);
    outCharacter.maxHitPoints   = ReadBcd (roster, at + kMaxHitsAt,      kWordBytes);
    outCharacter.experience     = ReadBcd (roster, at + kExperienceAt,   kWordBytes);
    outCharacter.level          = ReadBcd (roster, at + kLevelAt,        1);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::IsFound
//
//  The first chunk of the world is all deep water, as Britannia's
//  northwest corner is, and the volume has ROSTER and PRTY.DATA with a
//  character in the first slot. The chunk is checked first, so another
//  ProDOS disk is turned away before its catalog is read.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima5Save::IsFound (const std::vector<Byte> & sectors)
{
    constexpr Byte     kDeepWater = 0x01;
    std::vector<Byte>  roster;
    std::vector<Byte>  party;
    Character          character;
    bool               isWater    = sectors.size() == kImageBytes;



    for (int y = 0; isWater && y < kRegionSide; y++)
    {
        for (int x = 0; isWater && x < kRegionSide; x++)
        {
            isWater = GetTile (sectors, x, y) == kDeepWater;
        }
    }

    return isWater && TryReadFiles (sectors, roster, party) && TryReadCharacter (roster, 0, character);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::GetTileName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima5Save::GetTileName (Byte tile)
{
    for (const auto & known : s_kTiles)
    {
        if (tile >= known.first && tile <= known.last)
        {
            return known.pszName;
        }
    }

    return std::format (L"Tile {:02X}", tile);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::GetTileColor
//
////////////////////////////////////////////////////////////////////////////////

uint32_t Ultima5Save::GetTileColor (Byte tile)
{
    for (const auto & known : s_kTiles)
    {
        if (tile >= known.first && tile <= known.last)
        {
            return known.argb;
        }
    }

    return s_kOtherTile;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::RenderMap
//
//  Each tile s_kTilePixels square in its color, with a tip of its column
//  and row and its name. The party is a white square with a black edge,
//  three tiles wide so it is visible at this scale, and the tiles a sector that
//  couldn't be read covers are striped.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima5Save::RenderMap (const std::vector<Byte> & sectors, std::span<const bool> unreadable, int partyX, int partyY, Paragraph & ioMap)
{
    constexpr int  kMarkTiles      = 3;
    constexpr int  kStripeWidth    = 2;
    int            side            = kWorldSide * s_kTilePixels;
    int            markLeft        = (partyX - kMarkTiles / 2) * s_kTilePixels;
    int            markTop         = (partyY - kMarkTiles / 2) * s_kTilePixels;
    int            markSide        = kMarkTiles * s_kTilePixels;



    ioMap.width      = side;
    ioMap.height     = side;
    ioMap.tipCell    = s_kTilePixels;
    ioMap.tipLeft    = 0;
    ioMap.tipTop     = 0;
    ioMap.tipColumns = kWorldSide;
    ioMap.pingX      = partyX * s_kTilePixels + s_kTilePixels / 2;
    ioMap.pingY      = partyY * s_kTilePixels + s_kTilePixels / 2;
    ioMap.canZoom    = true;
    ioMap.picture.assign ((size_t) side * (size_t) side, s_kOtherTile);
    ioMap.tips.assign ((size_t) kWorldSide * (size_t) kWorldSide, PictureTip());

    for (int y = 0; y < kWorldSide; y++)
    {
        for (int x = 0; x < kWorldSide; x++)
        {
            Byte          tile    = GetTile (sectors, x, y);
            int           chunk   = (y / kRegionSide) * kRegionSide + (x / kRegionSide);
            size_t        offset  = ProDosSkeleton::GetBlockByteOffset (sectors.size(), kMapFirstBlock + chunk / 2, (size_t) (chunk % 2) * kChunkBytes);
            size_t        sector  = offset / kChunkBytes;
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
//  Ultima5Save::AddLegend
//
//  A color sample and name for the party and each kind of place, laid out
//  in columns.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima5Save::AddLegend (std::vector<Paragraph> & ioParagraphs)
{
    std::vector<Run>  runs;
    Paragraph         legend;



    runs.push_back (Run { L"Party", false, false, false, RunColor::Muted, s_kPartyMark });

    for (const auto & known : s_kTiles)
    {
        runs.push_back (Run { known.pszName, false, false, false, RunColor::Muted, known.argb });
    }

    runs.push_back (Run { L"Other",            false, false, false, RunColor::Muted, s_kOtherTile });
    runs.push_back (Run { L"Couldn't be read", false, false, false, RunColor::Muted, kHatchedSwatch });

    legend.runs     = std::move (runs);
    legend.isLegend = true;
    ioParagraphs.push_back (std::move (legend));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::GetClassName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima5Save::GetClassName (wchar_t letter)
{
    switch (letter)
    {
        case L'A': return L"Avatar";
        case L'F': return L"Fighter";
        case L'B': return L"Bard";
        case L'M': return L"Mage";
        default:   return std::wstring (1, letter);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::GetStatusName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima5Save::GetStatusName (wchar_t letter)
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
//  Ultima5Save::MakeParagraph
//
////////////////////////////////////////////////////////////////////////////////

Paragraph Ultima5Save::MakeParagraph (std::vector<Run> runs)
{
    Paragraph  paragraph;



    paragraph.runs = std::move (runs);

    return paragraph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::MakeLabel
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima5Save::MakeLabel (const std::wstring & text)
{
    return Run { text, false, false, false, RunColor::Muted };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::MakeValue
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima5Save::MakeValue (const std::wstring & text)
{
    return Run { text, true, false, false, RunColor::Keyword };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::GetHealthColor
//
//  Green at full hit points, red at a quarter or less, and the text color
//  between.
//
////////////////////////////////////////////////////////////////////////////////

RunColor Ultima5Save::GetHealthColor (int hitPoints, int maxHitPoints)
{
    constexpr int  kLowQuarters = 4;



    return (maxHitPoints > 0 && hitPoints >= maxHitPoints) ? RunColor::Good
         : (hitPoints * kLowQuarters <= maxHitPoints)      ? RunColor::Bad
                                                           : RunColor::Text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::AddCharacter
//
//  A character as a table row, and as a card of five short lines for a
//  narrow pane: the name and the class and sex, the status, hit and magic
//  points, the stats, the level and experience, and whether it's in the
//  party.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima5Save::AddCharacter (const Character & character, RecordTable & ioTable)
{
    std::wstring         sex       = character.isFemale ? L"female" : L"male";
    RunColor             status    = (character.status == L'G') ? RunColor::Good : RunColor::Bad;
    RunColor             nameColor = (character.status == L'D') ? RunColor::Bad  : RunColor::Text;
    RunColor             health    = GetHealthColor (character.hitPoints, character.maxHitPoints);
    RecordTable::Record  record;



    record.cells = { Run { character.name, true, false, false, nameColor },
                     Run { GetClassName (character.characterClass) },
                     Run { sex },
                     Run { GetStatusName (character.status), true, false, false, status },
                     Run { std::format (L"{} / {} / {}", character.strength, character.dexterity, character.intelligence) },
                     Run { std::format (L"{} / {}", character.hitPoints, character.maxHitPoints), true, false, false, health },
                     Run { std::format (L"{}", character.magic) },
                     Run { std::format (L"{}", character.level) },
                     Run { std::format (L"{}", character.experience) } };

    record.card.push_back (MakeParagraph ({ Run { character.name, true, false, false, nameColor },
                                            Run { std::format (L"   {}, {}", GetClassName (character.characterClass), sex), false, true, false, RunColor::Muted } }));
    record.card.push_back (MakeParagraph ({ Run { GetStatusName (character.status), true, false, false, status },
                                            MakeLabel (L"   HP "), Run { std::format (L"{} / {}", character.hitPoints, character.maxHitPoints), true, false, false, health },
                                            MakeLabel (L"   MP "), MakeValue (std::format (L"{}", character.magic)) }));
    record.card.push_back (MakeParagraph ({ MakeLabel (L"Str "),    MakeValue (std::format (L"{}", character.strength)),
                                            MakeLabel (L"   Dex "), MakeValue (std::format (L"{}", character.dexterity)),
                                            MakeLabel (L"   Int "), MakeValue (std::format (L"{}", character.intelligence)) }));
    record.card.push_back (MakeParagraph ({ MakeLabel (L"Level "),  MakeValue (std::format (L"{}", character.level)),
                                            MakeLabel (L"   Exp "), MakeValue (std::format (L"{}", character.experience)) }));

    ioTable.records.push_back (std::move (record));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima5Save::Decode
//
//  The title and date, the world with the party marked, the legend, the
//  party and the rest of the roster, and the food and gold.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ultima5Save::Decode (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError)
{
    constexpr size_t   kXAt       = 0x00;
    constexpr size_t   kYAt       = 0x01;
    constexpr size_t   kYearAt    = 0x1A;
    constexpr size_t   kMonthAt   = 0x1C;
    constexpr size_t   kDayAt     = 0x1D;
    constexpr size_t   kHourAt    = 0x1E;
    constexpr size_t   kMinuteAt  = 0x1F;
    constexpr size_t   kWordBytes = 2;
    HRESULT            hr         = S_OK;
    std::vector<Byte>  roster;
    std::vector<Byte>  party;
    Character          character;
    Paragraph          map;
    Paragraph          members;
    Paragraph          others;
    auto               inParty    = std::make_shared<RecordTable>();
    auto               rest       = std::make_shared<RecordTable>();
    bool               isFound    = IsFound (sectors);
    bool               hasFiles   = false;
    int                x          = 0;
    int                y          = 0;



    outFile        = DecodedFile();
    outFile.format = FileFormat::UltimaSave;
    outFile.kind   = DecodedFile::Kind::Document;

    CBRFEx (isFound, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The disk doesn't have an Ultima V saved game.");

    hasFiles = TryReadFiles (sectors, roster, party);
    CBRAEx (hasFiles, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    x = party[kXAt];
    y = party[kYAt];

    RenderMap (sectors, unreadable, x, y, map);

    outFile.paragraphs.push_back (MakeParagraph ({ Run { L"Ultima V: Warriors of Destiny", true } }));
    outFile.paragraphs.push_back (MakeParagraph ({ Run { std::format (L"Saved game, the party at column {}, row {}, on {}-{}-{} at {}:{:02}", x, y,
                                                                      ReadBcd (party, kMonthAt, 1), ReadBcd (party, kDayAt, 1), ReadBcd (party, kYearAt, kWordBytes),
                                                                      ReadBcd (party, kHourAt, 1), ReadBcd (party, kMinuteAt, 1)),
                                                        false, false, false, RunColor::Muted } }));
    outFile.paragraphs.push_back (std::move (map));
    AddLegend (outFile.paragraphs);

    inParty->title    = L"Party";
    rest->title       = L"Roster";
    inParty->headings = { L"Name", L"Class", L"Sex", L"Status", L"Str / Dex / Int", L"HP", L"MP", L"Level", L"Exp" };
    rest->headings    = inParty->headings;

    for (int i = 0; i < kCharacters; i++)
    {
        if (TryReadCharacter (roster, i, character))
        {
            AddCharacter (character, character.inParty ? *inParty : *rest);
        }
    }

    members.table = inParty;
    others.table  = rest;

    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (std::move (members));
    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (std::move (others));
    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Food "),    MakeValue (std::format (L"{}", ReadBcd (roster, kFoodAt, kWordBytes))),
                                                   MakeLabel (L"   Gold "), MakeValue (std::format (L"{}", ReadBcd (roster, kGoldAt, kWordBytes))) }));

Error:
    return hr;
}
