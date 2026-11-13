#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenario
//
//  The SCENARIO.DATA file of Wizardry I, II and III: its monsters, items
//  and characters as lists, and each maze level as a map of its walls,
//  doors and special squares.
//
//  The file is records of UCSD Pascal types. Block 0 is the table of
//  contents: the scenario's name as a STRING[40], then four arrays of eight
//  words, one word for each zone (none, mazes, monsters, rewards, items,
//  characters, special characters, experience): the records in each pair of
//  blocks, the records in the zone, a word unused, and the zone's first
//  block. The names of the races, classes, statuses and alignments follow
//  as STRING[9] and STRING[8], each padded to a whole word. Record n of a
//  zone is in the two blocks at its first block plus 2 * (n div records per
//  pair), at n mod records per pair times the record's size.
//
//  The record types are from Thomas William Ewers' re-engineered Pascal
//  source of Wizardry I. A string is a length byte and its characters; an
//  integer, a Boolean and an enumeration are each a word, low byte first;
//  a packed array puts its elements in whole words from bit 0, none across
//  a word; and a money or experience amount is three words of four decimal
//  digits each, low first. A maze level is 20 by 20 squares, x east and y
//  north, with a wall on each side of each square, open, a wall, a door or
//  a hidden door, and a special square type from a table of 16 for each.
//
//  A file matches by its name and a table of contents whose zones fit the
//  record sizes.
//
////////////////////////////////////////////////////////////////////////////////

class WizardryScenario
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    enum Zone
    {
        kZoneMaze      = 1,
        kZoneMonster   = 2,
        kZoneReward    = 3,
        kZoneItem      = 4,
        kZoneCharacter = 5,
        kZoneCount     = 8,
    };

    static constexpr size_t  kBlockBytes     = 512;
    static constexpr size_t  kPairBytes      = 1024;
    static constexpr size_t  kMazeBytes      = 894;
    static constexpr size_t  kMonsterBytes   = 158;
    static constexpr size_t  kItemBytes      = 78;
    static constexpr size_t  kCharacterBytes = 208;
    static constexpr int     kMazeSide       = 20;

    //  The table of contents' fields.
    struct Contents
    {
        std::wstring               name;
        int                        perPair[kZoneCount]    = {};
        int                        count[kZoneCount]      = {};
        int                        firstBlock[kZoneCount] = {};
        std::vector<std::wstring>  races;
        std::vector<std::wstring>  classes;
        std::vector<std::wstring>  statuses;
        std::vector<std::wstring>  alignments;
    };

    static bool  ReadContents (std::span<const Byte> bytes, Contents & outContents);

    //  Where record n of a zone begins, or false when it isn't in the file.
    static bool  TryLocate    (std::span<const Byte> bytes, const Contents & contents, int zone, int index, size_t size, size_t & outAt);

    //  A maze level as lines of text, north at the top.
    static void  DrawLevel    (std::span<const Byte> level, std::vector<std::wstring> & outLines);

private:
    static constexpr size_t  kNameField        = 40;
    static constexpr size_t  kNameWords        = 42;
    static constexpr size_t  kZoneArrayBytes   = 16;
    static constexpr size_t  kPerPairAt        = kNameWords;
    static constexpr size_t  kCountAt          = kPerPairAt + kZoneArrayBytes;
    static constexpr size_t  kFirstBlockAt     = kCountAt + 2 * kZoneArrayBytes;
    static constexpr size_t  kRacesAt          = kFirstBlockAt + kZoneArrayBytes;
    static constexpr size_t  kNameSlotBytes    = 10;
    static constexpr int     kRaceCount        = 6;
    static constexpr int     kClassCount       = 8;
    static constexpr int     kStatusCount      = 8;
    static constexpr int     kAlignmentCount   = 4;
    static constexpr size_t  kStringBytes      = 16;
    static constexpr size_t  kShortStringMax   = 15;
    static constexpr int     kDigitsPerWord    = 10000;
    static constexpr int     kWallBits         = 2;
    static constexpr int     kFightBits        = 1;
    static constexpr int     kExtraBits        = 4;
    static constexpr size_t  kWallRowBytes     = 6;
    static constexpr size_t  kFightRowBytes    = 4;
    static constexpr size_t  kExtraRowBytes    = 10;
    static constexpr size_t  kWestAt           = 0;
    static constexpr size_t  kSouthAt          = 120;
    static constexpr size_t  kEastAt           = 240;
    static constexpr size_t  kNorthAt          = 360;
    static constexpr size_t  kFightsAt         = 480;
    static constexpr size_t  kExtraAt          = 560;
    static constexpr size_t  kSquareTypesAt    = 760;
    static constexpr int     kWordBits         = 16;

    static int           ReadWord     (std::span<const Byte> bytes, size_t at);
    static uint64_t      ReadAmount   (std::span<const Byte> bytes, size_t at);
    static std::wstring  ReadString   (std::span<const Byte> bytes, size_t at, size_t maximum);
    static int           ReadPacked   (std::span<const Byte> bytes, size_t at, size_t rowBytes, int bits, int x, int y);
    static std::wstring  GetName      (const std::vector<std::wstring> & names, int index);

    static void  ListMonsters   (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes);
    static void  ListItems      (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes);
    static void  ListCharacters (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes);
    static void  ListMazes      (std::span<const Byte> bytes, const Contents & contents, std::vector<std::wstring> & ioLines, std::vector<std::wstring> & ioNotes);
};
