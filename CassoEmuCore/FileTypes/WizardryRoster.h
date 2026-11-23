#pragma once

#include "Pch.h"

#include "FileTypes/FileFormatTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster
//
//  The PLAYER.DATA file of Wizardry V as the saved game: the roster of
//  characters. Unlike SCENARIO.DATA of Wizardry I, II and III, the file has
//  only the characters, so the race, class, status and alignment
//  labels are the ones the game displays.
//
//  The file is records of kCharacterBytes, back to back across its blocks.
//  A record has the name and the password, each a STRING[15]; the race,
//  class, alignment, status and age in weeks, each a word; the six
//  attributes, five bits each, three to a word; the saving throws; the gold
//  as three words of four decimal digits each, low first; the item count
//  and the items; the experience as the gold; the highest level, the level,
//  and the hit points left and most. A slot not in use has the status lost.
//
//  Wizardry IV's WERDNA.DATA has Werdna's record once for each of the eight
//  save slots, one to a block, in Wizardry I's layout: the race, class,
//  age, status and alignment from offset 34, the gold, the item count and
//  the items, the experience, which the game calls keys, and the level and
//  hit points. A slot not saved to has Werdna as the game begins, at level
//  0.
//
//  A file matches by its name and records whose fields fit.
//
////////////////////////////////////////////////////////////////////////////////

class WizardryRoster
{
public:
    static bool     Matches (const FileFacts & facts, std::span<const Byte> bytes);
    static HRESULT  Decode  (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError);

    static constexpr size_t  kCharacterBytes = 246;
    static constexpr size_t  kNameAt         = 0;
    static constexpr size_t  kRaceAt         = 32;
    static constexpr size_t  kClassAt        = 34;
    static constexpr size_t  kAlignAt        = 36;
    static constexpr size_t  kStatusAt       = 40;
    static constexpr size_t  kAgeAt          = 42;
    static constexpr size_t  kGoldAt         = 52;
    static constexpr size_t  kExperienceAt   = 92;
    static constexpr size_t  kLevelAt        = 100;
    static constexpr size_t  kHpLeftAt       = 102;
    static constexpr size_t  kHpMaxAt        = 104;
    static constexpr int     kStatusLost     = 7;

    static constexpr size_t  kSlotBytes      = 512;
    static constexpr int     kSlots          = 8;
    static constexpr size_t  kWAgeAt         = 38;
    static constexpr size_t  kWStatusAt      = 40;
    static constexpr size_t  kWGoldAt        = 52;
    static constexpr size_t  kWItemCountAt   = 58;
    static constexpr size_t  kWKeysAt        = 124;
    static constexpr size_t  kWLevelAt       = 132;
    static constexpr size_t  kWHpLeftAt      = 134;
    static constexpr size_t  kWHpMaxAt       = 136;

private:
    static constexpr size_t  kShortStringMax = 15;
    static constexpr int     kWeeksPerYear   = 52;
    static constexpr int     kMaxRace        = 5;
    static constexpr int     kMaxClass       = 7;
    static constexpr int     kMaxAlign       = 3;
    static constexpr int     kMaxStatus      = 7;

    static bool          IsRecord      (std::span<const Byte> bytes, size_t at);
    static bool          IsPlayerData  (const FileFacts & facts, std::span<const Byte> bytes);
    static bool          IsWerdnaData  (const FileFacts & facts, std::span<const Byte> bytes);
    static void          ListSlots     (std::span<const Byte> bytes, std::vector<Paragraph> & ioParagraphs);
    static std::wstring  GetRaceName   (int race);
    static std::wstring  GetClassName  (int kind);
    static std::wstring  GetAlignName  (int align);
    static std::wstring  GetStatusName (int status);
    static void          ListRoster    (std::span<const Byte> bytes, std::vector<Paragraph> & ioParagraphs);
};
