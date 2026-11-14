#include "Pch.h"

#include "FileTypes/WizardryScenario.h"
#include "Core/UnicodeSymbols.h"





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

//  Each scenario's title, by the name in its table of contents.
static constexpr struct { const wchar_t * name; const wchar_t * title; }  s_kTitles[] =
{
    { L"PROVING GROUNDS OF THE MAD OVERLORD!", L"Wizardry I: Proving Grounds of the Mad Overlord" },
    { L"THE KNIGHT OF DIAMONDS",               L"Wizardry II: The Knight of Diamonds"            },
    { L"THE LEGACY OF LLYLGAMYN",              L"Wizardry III: Legacy of Llylgamyn"              },
};

//  The mark of an item that isn't identified, before its unknown name.
static constexpr const wchar_t *  s_kpszUnknownMark = L"?";

//  The map's square size and border, and its colors, as 0xAARRGGBB.
static constexpr int       s_kMapCell        = 24;
static constexpr int       s_kMapBorder      = 4;
static constexpr int       s_kWallPixels     = 3;
static constexpr int       s_kDoorPixels     = 7;
static constexpr uint32_t  s_kMapPaper       = 0xFFFFFFFF;
static constexpr uint32_t  s_kMapGrid        = 0xFFE6E6E6;
static constexpr uint32_t  s_kMapWall        = 0xFF202020;
static constexpr uint32_t  s_kMapDoor        = 0xFFC0601A;
static constexpr uint32_t  s_kMapFight       = 0xFFFBE0E0;
static constexpr uint32_t  s_kMapStairs      = 0xFFCFE0FF;
static constexpr uint32_t  s_kMapDark        = 0xFFA8A8A8;
static constexpr uint32_t  s_kMapRock        = 0xFF6E6E6E;
static constexpr uint32_t  s_kMapSpecial     = 0xFF8A8A8A;
static constexpr uint32_t  s_kMapParty       = 0xFFE0A800;
static constexpr uint32_t  s_kMapPartyEdge   = 0xFF7A5C00;

//  The special square types the map fills: stairs, darkness, and rock or
//  water.
static constexpr int  s_kSquareStairs   = 1;
static constexpr int  s_kSquareDarkness = 5;
static constexpr int  s_kSquareRock     = 9;

//  A side of a square: open, a wall, a door or a hidden door.
static constexpr int  s_kSideWall       = 1;
static constexpr int  s_kSideDoor       = 2;
static constexpr int  s_kSideHiddenDoor = 3;

static constexpr const wchar_t *  s_kpszMapLegend = L"The party is the gold circle. Walls are black, doors are brown, and hidden doors are dotted brown. "
                                                    L"Blue squares are stairs, gray ones are dark, dark gray ones are rock or water, and pink ones are "
                                                    L"fight zones. A dot marks any other special square.";





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
//  Each character with a slot in use as a short block, which reads in a
//  narrow pane: the name in bold and the level, race, class and alignment;
//  the status, hit points, gold and experience; and a line for each item
//  in its inventory, a green check for one equipped, a red skull for one
//  cursed, and a question mark and the unknown name, in gray italics, for
//  one not identified. A character in the maze has the game's status OUT,
//  and one with a status other than OK is in red.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::ListRoster (std::span<const Byte> bytes, const Contents & contents, std::vector<Paragraph> & ioParagraphs, std::vector<std::wstring> & ioNotes)
{
    std::vector<Paragraph>  roster;
    int                     listed = 0;



    for (int i = 0; i < contents.count[kZoneCharacter]; i++)
    {
        size_t        at     = 0;
        int           status = 0;
        int           items  = 0;
        bool          inMaze = false;
        std::wstring  shown;

        if (!TryLocate (bytes, contents, kZoneCharacter, i, kCharacterBytes, at))
        {
            ioNotes.push_back (std::format (L"The file ends before character {}.", i));
            break;
        }

        status = ReadWord (bytes, at + s_kCharStatusAt);
        inMaze = ReadWord (bytes, at + s_kCharInMazeAt) != 0;

        if (bytes[at + s_kCharNameAt] == 0 || status == s_kStatusLost)
        {
            continue;
        }

        shown = inMaze ? std::wstring (L"Out") : ToTitleCase (GetName (contents.statuses, status));

        roster.push_back (Paragraph());
        roster.push_back (MakeParagraph ({ Run { ReadString (bytes, at + s_kCharNameAt, kShortStringMax), true },
                                           Run { std::format (L"   Level {} {} {}, {}",
                                                              ReadWord (bytes, at + s_kCharLevelAt),
                                                              ToTitleCase (GetName (contents.races,      ReadWord (bytes, at + s_kCharRaceAt))),
                                                              ToTitleCase (GetName (contents.classes,    ReadWord (bytes, at + s_kCharClassAt))),
                                                              ToTitleCase (GetName (contents.alignments, ReadWord (bytes, at + s_kCharAlignAt)))),
                                                 false, false, false, RunColor::Muted } }));
        roster.push_back (MakeParagraph ({ Run { L"Status: " },
                                           Run { shown, false, false, false, (status == 0 && !inMaze) ? RunColor::Text : RunColor::Bad },
                                           Run { std::format (L"   Hit points: {}/{}   Gold: {}   Experience: {}",
                                                              ReadWord (bytes, at + s_kCharHpLeftAt), ReadWord (bytes, at + s_kCharHpMaxAt),
                                                              GroupDigits (ReadAmount (bytes, at + s_kCharGoldAt)),
                                                              GroupDigits (ReadAmount (bytes, at + s_kCharExperienceAt))) } }));

        items = (std::clamp) (ReadWord (bytes, at + s_kCharItemCountAt), 0, s_kMaxPossessions);

        for (int k = 0; k < items; k++)
        {
            size_t        item       = at + s_kCharItemsAt + (size_t) k * s_kPossessionBytes;
            bool          equipped   = ReadWord (bytes, item + s_kPossEquippedAt)   != 0;
            bool          cursed     = ReadWord (bytes, item + s_kPossCursedAt)     != 0;
            bool          identified = ReadWord (bytes, item + s_kPossIdentifiedAt) != 0;
            std::wstring  name       = GetItemName (bytes, contents, ReadWord (bytes, item + s_kPossItemAt), identified);
            Run           mark;

            mark = cursed      ? Run { std::wstring (s_kpszSkull) + L" ",       false, false, false, RunColor::Bad   }
                 : equipped    ? Run { std::wstring (s_kpszCheckMark) + L" ",   false, false, false, RunColor::Good  }
                 : !identified ? Run { std::wstring (s_kpszUnknownMark) + L" ", false, false, false, RunColor::Muted }
                               : Run { L"     " };

            roster.push_back (MakeParagraph ({ Run { L"      " }, mark,
                                               Run { name, false, !identified, false, cursed ? RunColor::Bad : identified ? RunColor::Text : RunColor::Muted } }));
        }

        listed++;
    }

    ioParagraphs.push_back (MakeParagraph ({ Run { std::format (L"Saved game: {} {}", listed, (listed == 1) ? L"character" : L"characters"), false, false, false, RunColor::Muted } }));

    if (listed > 0)
    {
        ioParagraphs.insert (ioParagraphs.end(), roster.begin(), roster.end());
        ioParagraphs.push_back (Paragraph());
        ioParagraphs.push_back (MakeParagraph ({ Run { std::wstring (s_kpszCheckMark)   + L" equipped     ", false, false, false, RunColor::Good  },
                                                 Run { std::wstring (s_kpszSkull)       + L" cursed     ",   false, false, false, RunColor::Bad   },
                                                 Run { std::wstring (s_kpszUnknownMark) + L" not identified, by the name it has until it is", false, true, false, RunColor::Muted } }));
    }
    else
    {
        ioParagraphs.push_back (Paragraph());
        ioParagraphs.push_back (MakeParagraph ({ Run { L"No characters." } }));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::MakeParagraph
//
////////////////////////////////////////////////////////////////////////////////

Paragraph WizardryScenario::MakeParagraph (std::vector<Run> runs)
{
    Paragraph  paragraph;



    paragraph.runs = std::move (runs);

    return paragraph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::ToTitleCase
//
//  A name from the game, in capitals, with each word's first letter a
//  capital and the rest lowercase: HUMAN as Human, P-LYZE as P-lyze. OK
//  and any other name of two letters or fewer is returned as it is.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryScenario::ToTitleCase (const std::wstring & name)
{
    std::wstring  text      = name;
    bool          wordStart = true;



    if (text.size() > 2)
    {
        for (wchar_t & c : text)
        {
            c         = wordStart ? c : (wchar_t) towlower (c);
            wordStart = c == L' ';
        }
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::GroupDigits
//
//  A number with a comma between each group of three digits.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryScenario::GroupDigits (uint64_t value)
{
    std::wstring  digits = std::to_wstring (value);
    std::wstring  text;



    for (size_t i = 0; i < digits.size(); i++)
    {
        if (i > 0 && (digits.size() - i) % 3 == 0)
        {
            text += L',';
        }

        text += digits[i];
    }

    return text;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::RenderLevel
//
//  North at the top, each square s_kMapCell pixels: a pink fight zone, a
//  blue stair square, a gray dark one and a dark gray rock or water one
//  filled, a dot on any other special square, and a gold circle on the
//  party's. Each side is drawn by its type in the level: a black wall, a brown
//  door in the middle of a wall, a dotted brown hidden door, or a faint
//  grid line where it is open.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::RenderLevel (std::span<const Byte> level, int partyX, int partyY, std::vector<uint32_t> & outBgra, int & outWidth, int & outHeight)
{
    int  side = kMazeSide * s_kMapCell + 2 * s_kMapBorder + 1;



    outWidth  = side;
    outHeight = side;
    outBgra.assign ((size_t) side * (size_t) side, s_kMapPaper);

    auto  fill = [&] (int x, int y, int w, int h, uint32_t argb)
    {
        for (int row = (std::max) (0, y); row < (std::min) (side, y + h); row++)
        {
            for (int column = (std::max) (0, x); column < (std::min) (side, x + w); column++)
            {
                outBgra[(size_t) row * (size_t) side + (size_t) column] = argb;
            }
        }
    };

    //  One side of a square, from (x, y) along its length, across or down.
    auto  drawSide = [&] (int x, int y, bool across, int kind)
    {

        for (int i = 0; i <= s_kMapCell; i++)
        {
            bool      middle = i > s_kMapCell / 4 && i < s_kMapCell - s_kMapCell / 4;
            uint32_t  argb   = (kind == s_kSideWall)                                             ? s_kMapWall
                             : (kind == s_kSideDoor)                                             ? (middle ? s_kMapDoor : s_kMapWall)
                             : (kind == s_kSideHiddenDoor)                                       ? (middle ? ((i / 2) % 2 == 0 ? s_kMapDoor : s_kMapPaper) : s_kMapWall)
                                                                                                 : s_kMapGrid;
            int       thick  = (kind == 0) ? 1 : (middle && kind != s_kSideWall) ? s_kDoorPixels : s_kWallPixels;

            if (across)
            {
                fill (x + i, y - thick / 2, 1, thick, argb);
            }
            else
            {
                fill (x - thick / 2, y + i, thick, 1, argb);
            }
        }
    };

    for (int y = 0; y < kMazeSide; y++)
    {
        for (int x = 0; x < kMazeSide; x++)
        {
            int   left  = s_kMapBorder + x * s_kMapCell;
            int   top   = s_kMapBorder + (kMazeSide - 1 - y) * s_kMapCell;
            int   extra = ReadPacked (level, kExtraAt, kExtraRowBytes, kExtraBits, x, y);
            int   type  = ReadPacked (level, kSquareTypesAt, 0, kExtraBits, 0, extra);
            bool  fight = ReadPacked (level, kFightsAt, kFightRowBytes, kFightBits, x, y) != 0;

            if (fight)
            {
                fill (left, top, s_kMapCell, s_kMapCell, s_kMapFight);
            }

            if (type == s_kSquareStairs)        { fill (left, top, s_kMapCell, s_kMapCell, s_kMapStairs); }
            else if (type == s_kSquareDarkness) { fill (left, top, s_kMapCell, s_kMapCell, s_kMapDark);   }
            else if (type == s_kSquareRock)     { fill (left, top, s_kMapCell, s_kMapCell, s_kMapRock);   }
            else if (type > 0)                  { fill (left + s_kMapCell / 2 - 2, top + s_kMapCell / 2 - 2, 5, 5, s_kMapSpecial); }

            if (x == partyX && y == partyY)
            {
                int  center = s_kMapCell / 2;
                int  radius = s_kMapCell / 3;

                for (int dy = -radius; dy <= radius; dy++)
                {
                    for (int dx = -radius; dx <= radius; dx++)
                    {
                        int  distance = dx * dx + dy * dy;

                        if (distance <= radius * radius)
                        {
                            fill (left + center + dx, top + center + dy, 1, 1, (distance >= (radius - 1) * (radius - 1)) ? s_kMapPartyEdge : s_kMapParty);
                        }
                    }
                }
            }
        }
    }

    //  The sides last, over the fills.
    for (int y = 0; y < kMazeSide; y++)
    {
        for (int x = 0; x < kMazeSide; x++)
        {
            int  left = s_kMapBorder + x * s_kMapCell;
            int  top  = s_kMapBorder + (kMazeSide - 1 - y) * s_kMapCell;

            drawSide (left,              top,              true,  ReadPacked (level, kNorthAt, kWallRowBytes, kWallBits, x, y));
            drawSide (left,              top + s_kMapCell, true,  ReadPacked (level, kSouthAt, kWallRowBytes, kWallBits, x, y));
            drawSide (left,              top,              false, ReadPacked (level, kWestAt,  kWallRowBytes, kWallBits, x, y));
            drawSide (left + s_kMapCell, top,              false, ReadPacked (level, kEastAt,  kWallRowBytes, kWallBits, x, y));
        }
    }
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
//  The maze level the party is on as a picture, with the party's square
//  marked, and what its colors indicate. Nothing when no character is in
//  the maze.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryScenario::DrawPartyLevel (std::span<const Byte> bytes, const Contents & contents, std::vector<Paragraph> & ioParagraphs, std::vector<std::wstring> & ioNotes)
{
    int        x     = 0;
    int        y     = 0;
    int        level = 0;
    size_t     at    = 0;
    Paragraph  map;



    if (!FindParty (bytes, contents, x, y, level))
    {
        return;
    }

    if (!TryLocate (bytes, contents, kZoneMaze, level - 1, kMazeBytes, at))
    {
        ioNotes.push_back (std::format (L"The file ends before maze level {}.", level));
        return;
    }

    RenderLevel (bytes.subspan (at, kMazeBytes), x, y, map.picture, map.width, map.height);

    ioParagraphs.push_back (Paragraph());
    ioParagraphs.push_back (MakeParagraph ({ Run { std::format (L"Maze level {}, the party at {} east, {} north", level, x, y), true } }));
    ioParagraphs.push_back (std::move (map));
    ioParagraphs.push_back (MakeParagraph ({ Run { s_kpszMapLegend, false, false, false, RunColor::Muted } }));
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario::GetTitle
//
//  The game's number in Roman numerals and its title, for a scenario with
//  a known name, or "Wizardry:" and the name as the file has it.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryScenario::GetTitle (const std::wstring & name)
{
    std::wstring  title = L"Wizardry: " + name;



    for (const auto & known : s_kTitles)
    {
        if (name == known.name)
        {
            title = known.title;
        }
    }

    return title;
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
    outFile.kind   = DecodedFile::Kind::Document;

    CBRFEx (isValid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The scenario's table of contents can't be read.");

    outFile.paragraphs.push_back (MakeParagraph ({ Run { GetTitle (contents.name), true } }));

    ListRoster     (bytes, contents, outFile.paragraphs, outFile.notes);
    DrawPartyLevel (bytes, contents, outFile.paragraphs, outFile.notes);

Error:
    return hr;
}
