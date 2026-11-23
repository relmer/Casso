#include "Pch.h"

#include "Machines/Apple2/Common/Ultima2Save.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::IsBcd
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima2Save::IsBcd (Byte value)
{
    constexpr Byte  kDigitMax = 9;
    constexpr int   kNibble   = 4;
    constexpr Byte  kLowMask  = 0x0F;



    return (value >> kNibble) <= kDigitMax && (value & kLowMask) <= kDigitMax;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::ReadBcd
//
//  `count` bytes of binary-coded decimal at `at`, the most significant
//  first: 03 99 as 399.
//
////////////////////////////////////////////////////////////////////////////////

int Ultima2Save::ReadBcd (std::span<const Byte> bytes, size_t at, size_t count)
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
//  Ultima2Save::TryParse
//
//  The name from $04 in high-bit letters ending at a zero; the sex at $14,
//  a high-bit F or M; the class at $15 and the race at $16, each from 0;
//  Str, Agi, Sta, Cha, Wis and Int from $19, a decimal byte each; then hit
//  points at $1F, food at $21 and gold at $26, two decimal bytes each; and
//  the column and row at $28 and $29. False for a sector that isn't a
//  character.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima2Save::TryParse (std::span<const Byte> record, Character & outCharacter)
{
    constexpr Byte    kHighBit    = 0x80;
    constexpr Byte    kFemale     = 'F' | 0x80;
    constexpr Byte    kMale       = 'M' | 0x80;
    constexpr Byte    kFirstGlyph = 0xC1;
    constexpr Byte    kLastGlyph  = 0xDA;
    constexpr size_t  kNameAt     = 0x04;
    constexpr size_t  kNameBytes  = 16;
    constexpr size_t  kSexAt      = 0x14;
    constexpr size_t  kClassAt    = 0x15;
    constexpr size_t  kRaceAt     = 0x16;
    constexpr size_t  kStatsAt    = 0x19;
    constexpr size_t  kStats      = 6;
    constexpr size_t  kHitsAt     = 0x1F;
    constexpr size_t  kFoodAt     = 0x21;
    constexpr size_t  kGoldAt     = 0x26;
    constexpr size_t  kXAt        = 0x28;
    constexpr size_t  kYAt        = 0x29;
    constexpr size_t  kWordBytes  = 2;
    constexpr int     kKinds      = 4;
    bool              isValid     = record.size() >= kRecordBytes
                                    && record[kNameAt] >= kFirstGlyph && record[kNameAt] <= kLastGlyph
                                    && (record[kSexAt] == kFemale || record[kSexAt] == kMale)
                                    && record[kClassAt] < kKinds && record[kRaceAt] < kKinds;



    for (size_t i = 0; isValid && i < kStats; i++)
    {
        isValid = IsBcd (record[kStatsAt + i]);
    }

    for (size_t at : { kHitsAt, kFoodAt, kGoldAt })
    {
        isValid = isValid && IsBcd (record[at]) && IsBcd (record[at + 1]);
    }

    if (!isValid)
    {
        return false;
    }

    outCharacter = Character();

    for (size_t i = 0; i < kNameBytes && record[kNameAt + i] != 0; i++)
    {
        outCharacter.name += (wchar_t) (record[kNameAt + i] & ~kHighBit);
    }

    outCharacter.isFemale       = record[kSexAt] == kFemale;
    outCharacter.characterClass = record[kClassAt];
    outCharacter.race           = record[kRaceAt];
    outCharacter.strength       = ReadBcd (record, kStatsAt,     1);
    outCharacter.agility        = ReadBcd (record, kStatsAt + 1, 1);
    outCharacter.stamina        = ReadBcd (record, kStatsAt + 2, 1);
    outCharacter.charisma       = ReadBcd (record, kStatsAt + 3, 1);
    outCharacter.wisdom         = ReadBcd (record, kStatsAt + 4, 1);
    outCharacter.intelligence   = ReadBcd (record, kStatsAt + 5, 1);
    outCharacter.hitPoints      = ReadBcd (record, kHitsAt, kWordBytes);
    outCharacter.food           = ReadBcd (record, kFoodAt, kWordBytes);
    outCharacter.gold           = ReadBcd (record, kGoldAt, kWordBytes);
    outCharacter.x              = record[kXAt];
    outCharacter.y              = record[kYAt];

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima2Save::IsFound (const std::vector<Byte> & sectors)
{
    Character  character;



    return sectors.size() == kImageBytes
        && TryParse (std::span<const Byte> (sectors).subspan (kRecordAt, kRecordBytes), character);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::GetRaceName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima2Save::GetRaceName (int race)
{
    switch (race)
    {
        case 0:  return L"Human";
        case 1:  return L"Elf";
        case 2:  return L"Dwarf";
        case 3:  return L"Hobbit";
        default: return std::format (L"Race {}", race);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::GetClassName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima2Save::GetClassName (int characterClass)
{
    switch (characterClass)
    {
        case 0:  return L"Fighter";
        case 1:  return L"Cleric";
        case 2:  return L"Wizard";
        case 3:  return L"Thief";
        default: return std::format (L"Class {}", characterClass);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::MakeParagraph
//
////////////////////////////////////////////////////////////////////////////////

Paragraph Ultima2Save::MakeParagraph (std::vector<Run> runs)
{
    Paragraph  paragraph;



    paragraph.runs = std::move (runs);

    return paragraph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::MakeLabel
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima2Save::MakeLabel (const std::wstring & text)
{
    return Run { text, false, false, false, RunColor::Muted };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::MakeValue
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima2Save::MakeValue (const std::wstring & text)
{
    return Run { text, true, false, false, RunColor::Keyword };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima2Save::Decode
//
//  The title and position, then the character: name, race, class and sex,
//  hit points, food and gold, and the six stats.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ultima2Save::Decode (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT    hr       = S_OK;
    Character  c;
    bool       isParsed = sectors.size() == kImageBytes
                          && TryParse (std::span<const Byte> (sectors).subspan (kRecordAt, kRecordBytes), c);



    UNREFERENCED_PARAMETER (unreadable);

    outFile        = DecodedFile();
    outFile.format = FileFormat::UltimaSave;
    outFile.kind   = DecodedFile::Kind::Document;

    CBRFEx (isParsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The disk doesn't have an Ultima II saved game.");

    outFile.paragraphs.push_back (MakeParagraph ({ Run { L"Ultima II: Revenge of the Enchantress", true } }));
    outFile.paragraphs.push_back (MakeParagraph ({ Run { std::format (L"Saved game, the player at column {}, row {}", c.x, c.y), false, false, false, RunColor::Muted } }));
    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (MakeParagraph ({ Run { c.name, true },
                                                   Run { std::format (L"   {} {}, {}", GetRaceName (c.race), GetClassName (c.characterClass), c.isFemale ? L"female" : L"male"), false, true, false, RunColor::Muted } }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"HP "),      MakeValue (std::format (L"{}", c.hitPoints)),
                                                   MakeLabel (L"   Food "), MakeValue (std::format (L"{}", c.food)),
                                                   MakeLabel (L"   Gold "), MakeValue (std::format (L"{}", c.gold)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Str "),     MakeValue (std::format (L"{}", c.strength)),
                                                   MakeLabel (L"   Agi "),  MakeValue (std::format (L"{}", c.agility)),
                                                   MakeLabel (L"   Sta "),  MakeValue (std::format (L"{}", c.stamina)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Cha "),     MakeValue (std::format (L"{}", c.charisma)),
                                                   MakeLabel (L"   Wis "),  MakeValue (std::format (L"{}", c.wisdom)),
                                                   MakeLabel (L"   Int "),  MakeValue (std::format (L"{}", c.intelligence)) }));

Error:
    return hr;
}
