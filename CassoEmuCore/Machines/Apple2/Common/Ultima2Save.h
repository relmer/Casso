#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save
//
//  An Ultima II player disk, whose character is one sector, track 4 sector
//  8, as the saved game: the character's race, class, sex, stats, hit
//  points, food, gold and position.
//
////////////////////////////////////////////////////////////////////////////////

class Ultima2Save
{
public:
    struct Character
    {
        std::wstring  name;
        bool          isFemale       = false;
        int           race           = 0;
        int           characterClass = 0;
        int           strength       = 0;
        int           agility        = 0;
        int           stamina        = 0;
        int           charisma       = 0;
        int           wisdom         = 0;
        int           intelligence   = 0;
        int           hitPoints      = 0;
        int           food           = 0;
        int           gold           = 0;
        int           x              = 0;
        int           y              = 0;
    };

    static constexpr size_t  kImageBytes  = 143360;
    static constexpr size_t  kRecordAt    = (4 * 16 + 8) * 256;
    static constexpr size_t  kRecordBytes = 256;

    static bool          IsFound      (const std::vector<Byte> & sectors);
    static HRESULT       Decode       (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError);
    static bool          TryParse     (std::span<const Byte> record, Character & outCharacter);
    static std::wstring  GetRaceName  (int race);
    static std::wstring  GetClassName (int characterClass);

private:
    static int        ReadBcd       (std::span<const Byte> bytes, size_t at, size_t count);
    static bool       IsBcd         (Byte value);
    static Run        MakeLabel     (const std::wstring & text);
    static Run        MakeValue     (const std::wstring & text);
    static Paragraph  MakeParagraph (std::vector<Run> runs);
};
