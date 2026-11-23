#pragma once

#include "Pch.h"

#include "FileTypes/DecodedFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save
//
//  A player disk of the 1981 Ultima, a DOS 3.3 disk whose catalog has a file
//  NAME=<character>, as the saved game. The game saves its Applesoft
//  variables to track 26, sectors 11 down to 0: array C has the hit points,
//  the six stats, gold, race and class, and the simple variables FO, EP, TX
//  and TY have food, experience and position.
//
////////////////////////////////////////////////////////////////////////////////

class Ultima1981Save
{
public:
    struct Character
    {
        std::wstring  name;
        int           hitPoints      = 0;
        int           strength       = 0;
        int           agility        = 0;
        int           stamina        = 0;
        int           charisma       = 0;
        int           wisdom         = 0;
        int           intelligence   = 0;
        int           gold           = 0;
        int           race           = 0;
        int           characterClass = 0;
        int           food           = 0;
        int           experience     = 0;
        int           x              = 0;
        int           y              = 0;
    };

    static bool          IsFound      (const std::vector<Byte> & sectors);
    static HRESULT       Decode       (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError);
    static bool          TryParse     (const std::vector<Byte> & sectors, Character & outCharacter);
    static double        ReadFloat    (std::span<const Byte> bytes, size_t at);
    static std::wstring  GetRaceName  (int race);
    static std::wstring  GetClassName (int characterClass);

private:
    static constexpr int     kSaveTrack    = 26;
    static constexpr int     kSaveSectors  = 12;
    static constexpr size_t  kSectorBytes  = 256;
    static constexpr size_t  kEntryBytes   = 7;
    static constexpr size_t  kStatsCount   = 10;
    static constexpr size_t  kSaveEndBytes = ((size_t) kSaveTrack * 16 + kSaveSectors) * kSectorBytes;

    static std::vector<Byte>  GetImage        (const std::vector<Byte> & sectors);
    static bool               TryFindStats    (std::span<const Byte> image, size_t & outAt);
    static bool               IsVariableName  (Byte first, Byte second);
    static bool               TryFindVariable (std::span<const Byte> image, size_t tableAt, size_t tableEnd, char first, char second, double & outValue);
    static bool               TryReadName     (const std::vector<Byte> & sectors, std::wstring & outName);
    static Run                MakeLabel       (const std::wstring & text);
    static Run                MakeValue       (const std::wstring & text);
    static Paragraph          MakeParagraph   (std::vector<Run> runs);
};
