#include "Pch.h"

#include "FileTypes/WizardryScenario.h"





static constexpr size_t  s_kItemNameAt         = 0;
static constexpr size_t  s_kItemUnknownAt      = 16;

static constexpr size_t  s_kCharNameAt         = 0;
static constexpr size_t  s_kCharInMazeAt       = 32;
static constexpr size_t  s_kCharRaceAt         = 34;
static constexpr size_t  s_kCharClassAt        = 36;
static constexpr size_t  s_kCharStatusAt       = 40;
static constexpr size_t  s_kCharAlignAt        = 42;
static constexpr size_t  s_kCharGoldAt         = 52;
static constexpr size_t  s_kCharItemCountAt    = 58;
static constexpr size_t  s_kCharItemsAt        = 60;
static constexpr size_t  s_kCharExperienceAt   = 124;
static constexpr size_t  s_kCharLevelAt        = 132;
static constexpr size_t  s_kCharHpLeftAt       = 134;
static constexpr size_t  s_kCharHpMaxAt        = 136;
static constexpr size_t  s_kCharLocationAt     = 200;

//  An item in a character's inventory: equipped, cursed and identified,
//  each a Boolean, and the item's record number.
static constexpr size_t  s_kPossessionBytes    = 8;
static constexpr size_t  s_kPossEquippedAt     = 0;
static constexpr size_t  s_kPossCursedAt       = 2;
static constexpr size_t  s_kPossIdentifiedAt   = 4;
static constexpr size_t  s_kPossItemAt         = 6;
static constexpr int     s_kMaxPossessions     = 8;

//  The status of a character whose slot is free. A character with this
//  status can't join a party.
static constexpr int     s_kStatusLost         = 7;

//  The two-letter mark of each special square type, NORMAL first.
static constexpr const wchar_t *  s_kpszSquareMarks[] = { L"  ", L"St", L"Pi", L"Ch", L"Sp", L"Dk", L"Tr", L"Ou", L"Bu", L"Rk", L"Fz", L"Ms", L"En" };

//  A side of a square: open, a wall, a door or a hidden door.
static constexpr wchar_t          s_kSideMarks[]     = { L' ', L'|', L'D', L'H' };
static constexpr const wchar_t *  s_kpszTopMarks[]   = { L"  ", L"--", L"DD", L"HH" };

static constexpr const wchar_t *  s_kpszLegend = L"@ the party, | and -- wall, D door, H hidden door, * fight zone, St stairs, Pi pit, Ch chute, Sp spinner, "
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
//  WizardryScenario::GetItemName
//
//  An item's name as it appears in a character's inventory: its true name
//  once identified, its unknown name before, such as SWORD for a LONG SWORD.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryScenario::GetItemName (std::span<const Byte> bytes, const Contents & contents, int item, bool identified)
{
    size_t        at   = 0;
    std::wstring  name = std::format (L"Item {}", item);



    if (item >= 0 && item < contents.count[kZoneItem] && TryLocate (bytes, contents, kZoneItem, item, kItemBytes, at))
    {
        name = ReadString (bytes, at + (identified ? s_kItemNameAt : s_kItemUnknownAt), kShortStringMax);
    }

    return name;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ListRoster
//
//  Each character with a slot in use, and under it the items in its
//  inventory, marked as in the game: * equipped, - cursed and ? not
//  identified. A character in the maze has the game's status OUT.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::ListRoster (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes)
{
    std::vector<std::wstring>  roster;
    int                        listed = 0;



    roster.push_back (std::format (L"{:<16} {:<7} {:<8} {:>5}  {:<7} {:>9}  {:>11}  {:>11}  {}",
                                   L"Name", L"Race", L"Class", L"Level", L"Status", L"Hit pts", L"Gold", L"Experience", L"Alignment"));

    for (int i = 0; i < contents.count[kZoneCharacter]; i++)
    {
        size_t        at     = 0;
        int           status = 0;
        int           items  = 0;
        std::wstring  shown;

        if (!TryLocate (bytes, contents, kZoneCharacter, i, kCharacterBytes, at))
        {
            ioNotes.push_back (std::format (L"The file ends before character {}.", i));
            break;
        }

        status = ReadWord (bytes, at + s_kCharStatusAt);

        if (bytes[at + s_kCharNameAt] == 0 || status == s_kStatusLost)
        {
            continue;
        }

        shown = (ReadWord (bytes, at + s_kCharInMazeAt) != 0) ? std::wstring (L"OUT") : GetName (contents.statuses, status);

        roster.push_back (std::format (L"{:<16} {:<7} {:<8} {:>5}  {:<7} {:>9}  {:>11}  {:>11}  {}",
                                       ReadString (bytes, at + s_kCharNameAt, kShortStringMax),
                                       GetName (contents.races,   ReadWord (bytes, at + s_kCharRaceAt)),
                                       GetName (contents.classes, ReadWord (bytes, at + s_kCharClassAt)),
                                       ReadWord (bytes, at + s_kCharLevelAt),
                                       shown,
                                       std::format (L"{}/{}", ReadWord (bytes, at + s_kCharHpLeftAt), ReadWord (bytes, at + s_kCharHpMaxAt)),
                                       ReadAmount (bytes, at + s_kCharGoldAt),
                                       ReadAmount (bytes, at + s_kCharExperienceAt),
                                       GetName (contents.alignments, ReadWord (bytes, at + s_kCharAlignAt))));

        items = (std::clamp) (ReadWord (bytes, at + s_kCharItemCountAt), 0, s_kMaxPossessions);

        for (int k = 0; k < items; k++)
        {
            size_t   item       = at + s_kCharItemsAt + (size_t) k * s_kPossessionBytes;
            bool     equipped   = ReadWord (bytes, item + s_kPossEquippedAt)   != 0;
            bool     cursed     = ReadWord (bytes, item + s_kPossCursedAt)     != 0;
            bool     identified = ReadWord (bytes, item + s_kPossIdentifiedAt) != 0;
            wchar_t  mark       = cursed ? L'-' : equipped ? L'*' : !identified ? L'?' : L' ';

            roster.push_back (std::format (L"    {}{}", mark, GetItemName (bytes, contents, ReadWord (bytes, item + s_kPossItemAt), identified)));
        }

        listed++;
    }

    ioLines.push_back (std::format (L"Wizardry saved game: {} {}", listed, (listed == 1) ? L"character" : L"characters"));
    ioLines.push_back (L"");

    if (listed > 0)
    {
        ioLines.insert (ioLines.end(), roster.begin(), roster.end());
        ioLines.push_back (L"");
        ioLines.push_back (L"* equipped, - cursed, ? not identified");
    }
    else
    {
        ioLines.push_back (L"No characters.");
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::DrawLevel
//
//  Each square is three characters wide: its west side, then the mark of
//  its special square type, or * in a fight zone, or @ at the party's
//  square. A line of north sides is above each row, and the east sides of
//  the last column and the south sides of the bottom row close the map.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::DrawLevel (std::span<const Byte> level, std::vector<std::wstring> & outLines, int partyX, int partyY)
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

            if (x == partyX && y == partyY)
            {
                cells += L"@ ";
            }
            else if (type > 0 && type < (int) std::size (s_kpszSquareMarks))
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
//  WizardryScenario::FindParty
//
//  Where the party is: the square a character's record saves, the place a
//  party that fell in the maze lies, as x, y and the level from 1. Only a
//  character with a slot in use counts.
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryScenario::FindParty (std::span<const Byte> bytes, const Contents & contents, int & outX, int & outY, int & outLevel)
{
    bool  found = false;



    for (int i = 0; !found && i < contents.count[kZoneCharacter]; i++)
    {
        size_t  at = 0;

        if (TryLocate (bytes, contents, kZoneCharacter, i, kCharacterBytes, at) && bytes[at + s_kCharNameAt] != 0
            && ReadWord (bytes, at + s_kCharStatusAt) != s_kStatusLost)
        {
            outX     = ReadWord (bytes, at + s_kCharLocationAt);
            outY     = ReadWord (bytes, at + s_kCharLocationAt + 2);
            outLevel = ReadWord (bytes, at + s_kCharLocationAt + 4);
            found    = outLevel >= 1 && outLevel <= contents.count[kZoneMaze]
                    && outX >= 0 && outX < kMazeSide && outY >= 0 && outY < kMazeSide;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::DrawPartyLevel
//
//  The maze level the party is on, with the party's square marked. Nothing
//  when no character is in the maze.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::DrawPartyLevel (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes)
{
    int                        x     = 0;
    int                        y     = 0;
    int                        level = 0;
    size_t                     at    = 0;
    std::vector<std::wstring>  map;



    if (!FindParty (bytes, contents, x, y, level))
    {
        return;
    }

    if (!TryLocate (bytes, contents, kZoneMaze, level - 1, kMazeBytes, at))
    {
        ioNotes.push_back (std::format (L"The file ends before maze level {}.", level));
        return;
    }

    DrawLevel (bytes.subspan (at, kMazeBytes), map, x, y);

    ioLines.push_back (L"");
    ioLines.push_back (std::format (L"MAZE LEVEL {}, the party at {} east, {} north", level, x, y));
    ioLines.insert (ioLines.end(), map.begin(), map.end());
    ioLines.push_back (L"");
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

    ListRoster     (bytes, contents, outFile.lines, outFile.notes);
    DrawPartyLevel (bytes, contents, outFile.lines, outFile.notes);

Error:
    return hr;
}
