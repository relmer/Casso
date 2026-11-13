#include "Pch.h"

#include "FileTypes/WizardryScenario.h"





static constexpr size_t  s_kMonsterUnknownAt = 0;
static constexpr size_t  s_kMonsterNameAt    = 32;
static constexpr size_t  s_kMonsterPluralAt  = 48;
static constexpr size_t  s_kMonsterDiceAt    = 72;
static constexpr size_t  s_kMonsterFacesAt   = 74;
static constexpr size_t  s_kMonsterAddAt     = 76;
static constexpr size_t  s_kMonsterClassAt   = 78;
static constexpr size_t  s_kMonsterArmorAt   = 80;
static constexpr size_t  s_kMonsterAttacksAt = 82;

static constexpr size_t  s_kItemNameAt       = 0;
static constexpr size_t  s_kItemUnknownAt    = 16;
static constexpr size_t  s_kItemTypeAt       = 32;
static constexpr size_t  s_kItemAlignAt      = 34;
static constexpr size_t  s_kItemCursedAt     = 36;
static constexpr size_t  s_kItemPriceAt      = 44;

static constexpr size_t  s_kCharNameAt       = 0;
static constexpr size_t  s_kCharRaceAt       = 34;
static constexpr size_t  s_kCharClassAt      = 36;
static constexpr size_t  s_kCharStatusAt     = 40;
static constexpr size_t  s_kCharAlignAt      = 42;
static constexpr size_t  s_kCharLevelAt      = 132;
static constexpr size_t  s_kCharHpLeftAt     = 134;
static constexpr size_t  s_kCharHpMaxAt      = 136;

static constexpr const wchar_t *  s_kpszItemTypes[] = { L"Weapon", L"Armor", L"Shield", L"Helmet", L"Gauntlets", L"Special", L"Misc" };

//  The two-letter mark of each special square type, NORMAL first.
static constexpr const wchar_t *  s_kpszSquareMarks[] = { L"  ", L"St", L"Pi", L"Ch", L"Sp", L"Dk", L"Tr", L"Ou", L"Bu", L"Rk", L"Fz", L"Ms", L"En" };

//  A side of a square: open, a wall, a door or a hidden door.
static constexpr wchar_t          s_kSideMarks[]     = { L' ', L'|', L'D', L'H' };
static constexpr const wchar_t *  s_kpszTopMarks[]   = { L"  ", L"--", L"DD", L"HH" };

static constexpr const wchar_t *  s_kpszLegend = L"| and -- wall, D door, H hidden door, * fight zone, St stairs, Pi pit, Ch chute, Sp spinner, "
                                                 L"Dk darkness, Tr teleport, Ou damage, Bu buttons, Rk rock or water, Fz no magic, Ms message, "
                                                 L"En encounter";





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ReadWord
//
//  Signed, as Pascal's INTEGER.
//
////////////////////////////////////////////////////////////////////////////////

int WizardryScenario::ReadWord (std::span<const Byte> bytes, size_t at)
{
    return (int) (int16_t) (bytes[at] | (bytes[at + 1] << 8));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ReadAmount
//
////////////////////////////////////////////////////////////////////////////////

uint64_t WizardryScenario::ReadAmount (std::span<const Byte> bytes, size_t at)
{
    uint64_t  low  = (uint64_t) std::max (0, ReadWord (bytes, at));
    uint64_t  mid  = (uint64_t) std::max (0, ReadWord (bytes, at + 2));
    uint64_t  high = (uint64_t) std::max (0, ReadWord (bytes, at + 4));



    return (high * kDigitsPerWord + mid) * kDigitsPerWord + low;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ReadString
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryScenario::ReadString (std::span<const Byte> bytes, size_t at, size_t maximum)
{
    size_t        length = std::min ((size_t) bytes[at], maximum);
    std::wstring  text;



    for (size_t i = 0; i < length; i++)
    {
        wchar_t  ch = (wchar_t) (bytes[at + 1 + i] & 0x7F);

        text.push_back ((ch < L' ' || ch == 0x7F) ? L'?' : ch);
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ReadPacked
//
//  Element y of row x of a packed two-dimensional array, each row whole
//  words, each element `bits` wide from bit 0 of its word.
//
////////////////////////////////////////////////////////////////////////////////

int WizardryScenario::ReadPacked (std::span<const Byte> bytes, size_t at, size_t rowBytes, int bits, int x, int y)
{
    int     perWord = kWordBits / bits;
    size_t  wordAt  = at + (size_t) x * rowBytes + 2 * (size_t) (y / perWord);
    int     word    = bytes[wordAt] | (bytes[wordAt + 1] << 8);



    return (word >> ((y % perWord) * bits)) & ((1 << bits) - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::GetName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryScenario::GetName (const std::vector<std::wstring> & names, int index)
{
    return (index >= 0 && index < (int) names.size()) ? names[(size_t) index] : std::format (L"{}", index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ReadContents
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryScenario::ReadContents (std::span<const Byte> bytes, Contents & outContents)
{
    bool    isValid = bytes.size() >= kBlockBytes && bytes[0] >= 1 && bytes[0] <= kNameField;
    size_t  at      = kRacesAt;



    outContents = Contents();

    if (!isValid)
    {
        return false;
    }

    outContents.name = ReadString (bytes, 0, kNameField);

    for (int zone = 0; zone < kZoneCount; zone++)
    {
        outContents.perPair[zone]    = ReadWord (bytes, kPerPairAt    + 2 * (size_t) zone);
        outContents.count[zone]      = ReadWord (bytes, kCountAt      + 2 * (size_t) zone);
        outContents.firstBlock[zone] = ReadWord (bytes, kFirstBlockAt + 2 * (size_t) zone);
    }

    for (int i = 0; i < kRaceCount; i++, at += kNameSlotBytes)
    {
        outContents.races.push_back (ReadString (bytes, at, kNameSlotBytes - 1));
    }

    for (int i = 0; i < kClassCount; i++, at += kNameSlotBytes)
    {
        outContents.classes.push_back (ReadString (bytes, at, kNameSlotBytes - 1));
    }

    for (int i = 0; i < kStatusCount; i++, at += kNameSlotBytes)
    {
        outContents.statuses.push_back (ReadString (bytes, at, kNameSlotBytes - 2));
    }

    for (int i = 0; i < kAlignmentCount; i++, at += kNameSlotBytes)
    {
        outContents.alignments.push_back (ReadString (bytes, at, kNameSlotBytes - 1));
    }

    //  Each zone's records must fit a pair of blocks.
    isValid = isValid && outContents.perPair[kZoneMaze]      >= 1 && (size_t) outContents.perPair[kZoneMaze]      * kMazeBytes      <= kPairBytes;
    isValid = isValid && outContents.perPair[kZoneMonster]   >= 1 && (size_t) outContents.perPair[kZoneMonster]   * kMonsterBytes   <= kPairBytes;
    isValid = isValid && outContents.perPair[kZoneItem]      >= 1 && (size_t) outContents.perPair[kZoneItem]      * kItemBytes      <= kPairBytes;
    isValid = isValid && outContents.perPair[kZoneCharacter] >= 1 && (size_t) outContents.perPair[kZoneCharacter] * kCharacterBytes <= kPairBytes;

    for (int zone = kZoneMaze; isValid && zone <= kZoneCharacter; zone++)
    {
        isValid = outContents.count[zone] >= 0 && outContents.firstBlock[zone] > 0;
    }

    return isValid;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::TryLocate
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryScenario::TryLocate (std::span<const Byte> bytes, const Contents & contents, int zone, int index, size_t size, size_t & outAt)
{
    int     perPair = contents.perPair[zone];
    size_t  block   = (size_t) contents.firstBlock[zone] + 2 * (size_t) (index / perPair);



    outAt = block * kBlockBytes + (size_t) (index % perPair) * size;

    return outAt + size <= bytes.size();
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryScenario::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    Contents  contents;
    bool      named = _stricmp (facts.name.c_str(), "SCENARIO.DATA") == 0;



    return named && ReadContents (bytes, contents);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ListMonsters
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::ListMonsters (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes)
{
    ioLines.push_back (L"MONSTERS");
    ioLines.push_back (std::format (L"{:>4}  {:<16} {:<16} {:<16} {:<10} {:>3}  {:>5}  {:>7}", L"#", L"Name", L"Plural", L"Unidentified", L"Hit points", L"AC", L"Class", L"Attacks"));

    for (int i = 0; i < contents.count[kZoneMonster]; i++)
    {
        size_t  at = 0;

        if (!TryLocate (bytes, contents, kZoneMonster, i, kMonsterBytes, at))
        {
            ioNotes.push_back (std::format (L"The file ends before monster {}.", i));
            break;
        }

        ioLines.push_back (std::format (L"{:>4}  {:<16} {:<16} {:<16} {:<10} {:>3}  {:>5}  {:>7}", i,
                                        ReadString (bytes, at + s_kMonsterNameAt,    kShortStringMax),
                                        ReadString (bytes, at + s_kMonsterPluralAt,  kShortStringMax),
                                        ReadString (bytes, at + s_kMonsterUnknownAt, kShortStringMax),
                                        std::format (L"{}d{}+{}", ReadWord (bytes, at + s_kMonsterDiceAt), ReadWord (bytes, at + s_kMonsterFacesAt), ReadWord (bytes, at + s_kMonsterAddAt)),
                                        ReadWord (bytes, at + s_kMonsterArmorAt),
                                        ReadWord (bytes, at + s_kMonsterClassAt),
                                        ReadWord (bytes, at + s_kMonsterAttacksAt)));
    }

    ioLines.push_back (L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ListItems
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::ListItems (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes)
{
    ioLines.push_back (L"ITEMS");
    ioLines.push_back (std::format (L"{:>4}  {:<16} {:<16} {:<9} {:>10}  {:<9} {}", L"#", L"Name", L"Unidentified", L"Type", L"Price", L"Alignment", L"Cursed"));

    for (int i = 0; i < contents.count[kZoneItem]; i++)
    {
        size_t  at   = 0;
        int     type = 0;

        if (!TryLocate (bytes, contents, kZoneItem, i, kItemBytes, at))
        {
            ioNotes.push_back (std::format (L"The file ends before item {}.", i));
            break;
        }

        type = ReadWord (bytes, at + s_kItemTypeAt);

        ioLines.push_back (std::format (L"{:>4}  {:<16} {:<16} {:<9} {:>10}  {:<9} {}", i,
                                        ReadString (bytes, at + s_kItemNameAt,    kShortStringMax),
                                        ReadString (bytes, at + s_kItemUnknownAt, kShortStringMax),
                                        (type >= 0 && type < (int) std::size (s_kpszItemTypes)) ? s_kpszItemTypes[type] : std::format (L"{}", type),
                                        ReadAmount (bytes, at + s_kItemPriceAt),
                                        GetName (contents.alignments, ReadWord (bytes, at + s_kItemAlignAt)),
                                        (ReadWord (bytes, at + s_kItemCursedAt) != 0) ? L"Yes" : L""));
    }

    ioLines.push_back (L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ListCharacters
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::ListCharacters (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes)
{
    ioLines.push_back (L"CHARACTERS");
    ioLines.push_back (std::format (L"{:>4}  {:<16} {:<9} {:<9} {:>5}  {:>9}  {:<8} {}", L"#", L"Name", L"Race", L"Class", L"Level", L"Hit pts", L"Status", L"Alignment"));

    for (int i = 0; i < contents.count[kZoneCharacter]; i++)
    {
        size_t  at = 0;

        if (!TryLocate (bytes, contents, kZoneCharacter, i, kCharacterBytes, at))
        {
            ioNotes.push_back (std::format (L"The file ends before character {}.", i));
            break;
        }

        if (bytes[at + s_kCharNameAt] == 0)
        {
            continue;
        }

        ioLines.push_back (std::format (L"{:>4}  {:<16} {:<9} {:<9} {:>5}  {:>9}  {:<8} {}", i,
                                        ReadString (bytes, at + s_kCharNameAt, kShortStringMax),
                                        GetName (contents.races,      ReadWord (bytes, at + s_kCharRaceAt)),
                                        GetName (contents.classes,    ReadWord (bytes, at + s_kCharClassAt)),
                                        ReadWord (bytes, at + s_kCharLevelAt),
                                        std::format (L"{}/{}", ReadWord (bytes, at + s_kCharHpLeftAt), ReadWord (bytes, at + s_kCharHpMaxAt)),
                                        GetName (contents.statuses,   ReadWord (bytes, at + s_kCharStatusAt)),
                                        GetName (contents.alignments, ReadWord (bytes, at + s_kCharAlignAt))));
    }

    ioLines.push_back (L"");
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::DrawLevel
//
//  Each square is three characters wide: its west side, then the mark of
//  its special square type, or * in a fight zone. A line of north sides is
//  above each row, and the east sides of the last column and the south
//  sides of the bottom row close the map.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::DrawLevel (std::span<const Byte> level, std::vector<std::wstring> & outLines)
{
    std::wstring  bottom = L"+";



    outLines.clear();

    for (int y = kMazeSide - 1; y >= 0; y--)
    {
        std::wstring  top   = L"+";
        std::wstring  cells;

        for (int x = 0; x < kMazeSide; x++)
        {
            int  extra = ReadPacked (level, kExtraAt, kExtraRowBytes, kExtraBits, x, y);
            int  type  = ReadPacked (level, kSquareTypesAt, 0, kExtraBits, 0, extra);
            bool fight = ReadPacked (level, kFightsAt, kFightRowBytes, kFightBits, x, y) != 0;

            top   += s_kpszTopMarks[ReadPacked (level, kNorthAt, kWallRowBytes, kWallBits, x, y)];
            top   += L"+";
            cells += s_kSideMarks[ReadPacked (level, kWestAt, kWallRowBytes, kWallBits, x, y)];

            if (type > 0 && type < (int) std::size (s_kpszSquareMarks))
            {
                cells += s_kpszSquareMarks[type];
            }
            else
            {
                cells += fight ? L"* " : L"  ";
            }
        }

        cells += s_kSideMarks[ReadPacked (level, kEastAt, kWallRowBytes, kWallBits, kMazeSide - 1, y)];

        outLines.push_back (top);
        outLines.push_back (cells);
    }

    for (int x = 0; x < kMazeSide; x++)
    {
        bottom += s_kpszTopMarks[ReadPacked (level, kSouthAt, kWallRowBytes, kWallBits, x, 0)];
        bottom += L"+";
    }

    outLines.push_back (bottom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ListMazes
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::ListMazes (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes)
{
    for (int i = 0; i < contents.count[kZoneMaze]; i++)
    {
        size_t                     at = 0;
        std::vector<std::wstring>  map;

        if (!TryLocate (bytes, contents, kZoneMaze, i, kMazeBytes, at))
        {
            ioNotes.push_back (std::format (L"The file ends before maze level {}.", i + 1));
            break;
        }

        DrawLevel (bytes.subspan (at, kMazeBytes), map);

        ioLines.push_back (std::format (L"MAZE LEVEL {}", i + 1));
        ioLines.insert (ioLines.end(), map.begin(), map.end());
        ioLines.push_back (L"");
    }

    ioLines.push_back (s_kpszLegend);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WizardryScenario::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT   hr      = S_OK;
    Contents  contents;
    bool      isValid = ReadContents (bytes, contents);



    UNREFERENCED_PARAMETER (facts);
    UNREFERENCED_PARAMETER (options);

    outFile        = DecodedFile();
    outFile.format = FileFormat::WizardryScenario;
    outFile.kind   = DecodedFile::Kind::Lines;

    CBRFEx (isValid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The scenario's table of contents can't be read.");

    outFile.lines.push_back (contents.name);
    outFile.lines.push_back (std::format (L"Wizardry scenario: {} maze levels, {} monsters, {} items, {} characters",
                                          contents.count[kZoneMaze], contents.count[kZoneMonster], contents.count[kZoneItem], contents.count[kZoneCharacter]));
    outFile.lines.push_back (L"");

    ListMonsters   (bytes, contents, outFile.lines, outFile.notes);
    ListItems      (bytes, contents, outFile.lines, outFile.notes);
    ListCharacters (bytes, contents, outFile.lines, outFile.notes);
    ListMazes      (bytes, contents, outFile.lines, outFile.notes);

Error:
    return hr;
}
