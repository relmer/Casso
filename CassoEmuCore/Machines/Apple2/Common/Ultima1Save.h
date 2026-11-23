#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save
//
//  An Ultima I: The Beginning player disk, a ProDOS disk with the file
//  U1.VARS, as the saved game: the character's race, class, sex, hit
//  points, stats, gold, food, position, readied weapon and armor, and
//  inventory. The world map is on the game disk, so it is drawn only when
//  the game disk is given.
//
////////////////////////////////////////////////////////////////////////////////

class Ultima1Save
{
public:
    struct Character
    {
        std::wstring  name;
        bool          isFemale       = false;
        int           race           = 0;
        int           characterClass = 0;
        int           hitPoints      = 0;
        int           strength       = 0;
        int           agility        = 0;
        int           stamina        = 0;
        int           charisma       = 0;
        int           wisdom         = 0;
        int           intelligence   = 0;
        int           gold           = 0;
        int           x              = 0;
        int           y              = 0;
        int           food           = 0;
        int           continent      = 0;

        std::wstring                               weapon;
        std::wstring                               armor;
        std::vector<std::pair<std::wstring, int>>  inventory;
    };

    static constexpr size_t  kVarsBytes = 0x80;

    static bool          IsFound       (const std::vector<Byte> & sectors);
    static HRESULT       Decode        (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError);
    static HRESULT       DecodeWithMap (const std::vector<Byte> & sectors, const std::vector<Byte> * gameSectors, DecodedFile & outFile, std::wstring & outError);
    static bool          TryParse      (std::span<const Byte> vars, Character & outCharacter);
    static std::wstring  GetRaceName   (int race);
    static std::wstring  GetClassName  (int characterClass);

private:
    static bool       TryReadVars   (const std::vector<Byte> & sectors, std::vector<Byte> & outVars);
    static void       AddCounts     (std::span<const Byte> vars, size_t at, std::span<const wchar_t * const> names, Character & ioCharacter);
    static int        ReadWord      (std::span<const Byte> bytes, size_t at);
    static Run        MakeLabel     (const std::wstring & text);
    static Run        MakeValue     (const std::wstring & text);
    static Paragraph  MakeParagraph (std::vector<Run> runs);
};
