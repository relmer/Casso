#include "Pch.h"

#include "Machines/Apple2/Common/Ultima1981Save.h"
#include "Machines/Apple2/Common/Dos33Volume.h"





//  The names of the counts in arrays PW, AR and SP, from 0. The first of
//  each, hands, skin and prayer, isn't an item and isn't listed.
static constexpr const wchar_t * s_kWeapons[] = { L"Hands", L"Dagger", L"Mace", L"Axe", L"Rope and spikes", L"Sword", L"Great sword",
                                                  L"Bow and arrows", L"Amulet", L"Wand", L"Staff", L"Triangle", L"Pistol",
                                                  L"Light sword", L"Phazor", L"Blaster" };
static constexpr const wchar_t * s_kArmor[]   = { L"Skin", L"Leather armor", L"Chain mail", L"Plate mail", L"Vacuum suit", L"Reflect suit" };
static constexpr const wchar_t * s_kSpells[]  = { L"Prayer", L"Open", L"Unlock", L"Magic missile", L"Steal", L"Ladder down", L"Ladder up",
                                                  L"Blink", L"Create", L"Destroy", L"Kill" };





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::GetImage
//
//  Track 26's sectors 11 down to 0, in the order the game saves memory to
//  them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<Byte> Ultima1981Save::GetImage (const std::vector<Byte> & sectors)
{
    constexpr int      kSectorsPerTrack = 16;
    std::vector<Byte>  image;



    for (int s = kSaveSectors - 1; s >= 0; s--)
    {
        size_t  at = ((size_t) kSaveTrack * kSectorsPerTrack + (size_t) s) * kSectorBytes;

        image.insert (image.end(), sectors.begin() + (ptrdiff_t) at, sectors.begin() + (ptrdiff_t) (at + kSectorBytes));
    }

    return image;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::ReadFloat
//
//  An Applesoft five-byte float: an exponent biased by 128, zero for zero,
//  then a mantissa whose top bit is the sign.
//
////////////////////////////////////////////////////////////////////////////////

double Ultima1981Save::ReadFloat (std::span<const Byte> bytes, size_t at)
{
    constexpr int     kBias       = 128;
    constexpr int     kMantBits   = 32;
    constexpr Byte    kSignBit    = 0x80;
    constexpr int     kByteBits   = 8;
    constexpr size_t  kMantBytes  = 4;
    uint64_t          mantissa    = 0;
    double            value       = 0;



    if (bytes[at] == 0)
    {
        return 0;
    }

    for (size_t i = 0; i < kMantBytes; i++)
    {
        Byte  b = bytes[at + 1 + i];

        if (i == 0)
        {
            b |= kSignBit;
        }

        mantissa = (mantissa << kByteBits) | b;
    }

    value = std::ldexp ((double) mantissa, bytes[at] - kBias - kMantBits);

    return (bytes[at + 1] & kSignBit) ? -value : value;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::IsVariableName
//
//  An Applesoft variable's two name bytes, each with its high bit set or
//  clear to mark its kind: a letter, then a letter, a digit or nothing.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1981Save::IsVariableName (Byte first, Byte second)
{
    constexpr Byte  kLowMask = 0x7F;
    Byte            a        = first  & kLowMask;
    Byte            b        = second & kLowMask;



    return a >= 'A' && a <= 'Z' && (b == 0 || (b >= 'A' && b <= 'Z') || (b >= '0' && b <= '9'));
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::TryFindStats
//
//  The offset of array C's values: the array begins the array table, a
//  header of the name C, its size, one dimension and the bound 10, then
//  eleven floats.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1981Save::TryFindStats (std::span<const Byte> image, size_t & outAt)
{
    static constexpr Byte  kHeader[] = { 'C', 0x00, 0x39, 0x00, 0x01, 0x00, 0x0A };



    for (size_t at = 0; at + sizeof (kHeader) + kStatsCount * 5 <= image.size(); at++)
    {
        if (std::equal (std::begin (kHeader), std::end (kHeader), image.begin() + (ptrdiff_t) at))
        {
            outAt = at;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::TryFindVariable
//
//  A simple float variable's value, from the table of seven-byte entries
//  between tableAt and tableEnd.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1981Save::TryFindVariable (std::span<const Byte> image, size_t tableAt, size_t tableEnd, char first, char second, double & outValue)
{
    for (size_t at = tableAt; at + kEntryBytes <= tableEnd; at += kEntryBytes)
    {
        if (image[at] == (Byte) first && image[at + 1] == (Byte) second)
        {
            outValue = ReadFloat (image, at + 2);
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::AddInventory
//
//  Walks the array table from array C, each array its two name bytes, its
//  size in bytes, its dimension count and bounds, then its values, and
//  adds each item in arrays PW, AR and SP whose count is more than zero.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima1981Save::AddInventory (std::span<const Byte> image, size_t statsAt, Character & ioCharacter)
{
    constexpr size_t  kHeaderBytes = 7;
    constexpr size_t  kFloatBytes  = 5;
    constexpr int     kByteBits    = 8;
    size_t            at           = statsAt;



    while (at + kHeaderBytes <= image.size() && IsVariableName (image[at], image[at + 1]))
    {
        size_t                            size  = image[at + 2] | ((size_t) image[at + 3] << kByteBits);
        std::span<const wchar_t * const>  names;

        if (size < kHeaderBytes || at + size > image.size())
        {
            break;
        }

        if (image[at] == 'P' && image[at + 1] == 'W')
        {
            names = s_kWeapons;
        }
        else if (image[at] == 'A' && image[at + 1] == 'R')
        {
            names = s_kArmor;
        }
        else if (image[at] == 'S' && image[at + 1] == 'P')
        {
            names = s_kSpells;
        }

        for (size_t i = 1; i < names.size() && kHeaderBytes + (i + 1) * kFloatBytes <= size; i++)
        {
            int  count = (int) ReadFloat (image, at + kHeaderBytes + i * kFloatBytes);

            if (count > 0)
            {
                ioCharacter.inventory.emplace_back (names[i], count);
            }
        }

        at += size;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::TryReadName
//
//  The character's name, from the catalog's file NAME=<name>.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1981Save::TryReadName (const std::vector<Byte> & sectors, std::wstring & outName)
{
    static constexpr char  kPrefix[] = "NAME=";
    constexpr size_t       kPrefixLen = sizeof (kPrefix) - 1;
    HRESULT                hr         = S_OK;
    VolumeListing          listing;



    hr = Dos33Volume (sectors).Enumerate (listing);
    CHR (hr);

    for (const FileEntry & entry : listing.entries)
    {
        if (entry.name.size() > kPrefixLen && entry.name.starts_with (kPrefix))
        {
            outName.assign (entry.name.begin() + (ptrdiff_t) kPrefixLen, entry.name.end());
            return true;
        }
    }

Error:
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::TryParse
//
//  The simple variables lie just before array C, so the table is found by
//  walking back from it a seven-byte entry at a time.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1981Save::TryParse (const std::vector<Byte> & sectors, Character & outCharacter)
{
    constexpr size_t   kFloatBytes        = 5;
    std::vector<Byte>  image;
    size_t             statsAt            = 0;
    size_t             tableAt            = 0;
    size_t             valuesAt           = 0;
    double             value[kStatsCount] = {};
    double             food               = 0;
    double             experience         = 0;
    double             x                  = 0;
    double             y                  = 0;
    Character          c;



    if (sectors.size() < kSaveEndBytes || !TryReadName (sectors, c.name))
    {
        return false;
    }

    image = GetImage (sectors);

    if (!TryFindStats (image, statsAt))
    {
        return false;
    }

    tableAt = statsAt;

    while (tableAt >= kEntryBytes && IsVariableName (image[tableAt - kEntryBytes], image[tableAt - kEntryBytes + 1]))
    {
        tableAt -= kEntryBytes;
    }

    if (!TryFindVariable (image, tableAt, statsAt, 'F', 'O', food)
        || !TryFindVariable (image, tableAt, statsAt, 'E', 'P', experience)
        || !TryFindVariable (image, tableAt, statsAt, 'T', 'X', x)
        || !TryFindVariable (image, tableAt, statsAt, 'T', 'Y', y))
    {
        return false;
    }

    valuesAt = statsAt + kEntryBytes;

    for (size_t i = 0; i < kStatsCount; i++)
    {
        value[i] = ReadFloat (image, valuesAt + i * kFloatBytes);
    }

    c.hitPoints      = (int) value[0];
    c.strength       = (int) value[1];
    c.agility        = (int) value[2];
    c.stamina        = (int) value[3];
    c.charisma       = (int) value[4];
    c.wisdom         = (int) value[5];
    c.intelligence   = (int) value[6];
    c.gold           = (int) value[7];
    c.race           = (int) value[8];
    c.characterClass = (int) value[9];
    c.food           = (int) food;
    c.experience     = (int) experience;
    c.x              = (int) x;
    c.y              = (int) y;

    AddInventory (image, statsAt, c);

    outCharacter = std::move (c);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1981Save::IsFound (const std::vector<Byte> & sectors)
{
    Character  character;



    return TryParse (sectors, character);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::GetRaceName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima1981Save::GetRaceName (int race)
{
    switch (race)
    {
        case 1:  return L"Human";
        case 2:  return L"Elf";
        case 3:  return L"Dwarf";
        case 4:  return L"Hobbit";
        default: return std::format (L"Race {}", race);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::GetClassName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima1981Save::GetClassName (int characterClass)
{
    switch (characterClass)
    {
        case 1:  return L"Fighter";
        case 2:  return L"Cleric";
        case 3:  return L"Wizard";
        case 4:  return L"Thief";
        default: return std::format (L"Class {}", characterClass);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::MakeParagraph
//
////////////////////////////////////////////////////////////////////////////////

Paragraph Ultima1981Save::MakeParagraph (std::vector<Run> runs)
{
    Paragraph  paragraph;



    paragraph.runs = std::move (runs);

    return paragraph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::MakeLabel
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima1981Save::MakeLabel (const std::wstring & text)
{
    return Run { text, false, false, false, RunColor::Muted };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::MakeValue
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima1981Save::MakeValue (const std::wstring & text)
{
    return Run { text, true, false, false, RunColor::Keyword };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1981Save::Decode
//
//  The title and position, then the character: name, race and class, hit
//  points, food, gold and experience, the six stats, and the inventory.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ultima1981Save::Decode (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT       hr       = S_OK;
    Character     c;
    std::wstring  items;
    bool          isParsed = TryParse (sectors, c);



    UNREFERENCED_PARAMETER (unreadable);

    outFile        = DecodedFile();
    outFile.format = FileFormat::UltimaSave;
    outFile.kind   = DecodedFile::Kind::Document;

    CBRFEx (isParsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The disk doesn't have an Ultima saved game.");

    outFile.paragraphs.push_back (MakeParagraph ({ Run { L"Ultima (1981)", true } }));
    outFile.paragraphs.push_back (MakeParagraph ({ Run { std::format (L"Saved game, the player at column {}, row {}", c.x, c.y), false, false, false, RunColor::Muted } }));
    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (MakeParagraph ({ Run { c.name, true },
                                                   Run { std::format (L"   {} {}", GetRaceName (c.race), GetClassName (c.characterClass)), false, true, false, RunColor::Muted } }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"HP "),      MakeValue (std::format (L"{}", c.hitPoints)),
                                                   MakeLabel (L"   Food "), MakeValue (std::format (L"{}", c.food)),
                                                   MakeLabel (L"   Gold "), MakeValue (std::format (L"{}", c.gold)),
                                                   MakeLabel (L"   Exp "),  MakeValue (std::format (L"{}", c.experience)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Str "),     MakeValue (std::format (L"{}", c.strength)),
                                                   MakeLabel (L"   Agi "),  MakeValue (std::format (L"{}", c.agility)),
                                                   MakeLabel (L"   Sta "),  MakeValue (std::format (L"{}", c.stamina)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Cha "),     MakeValue (std::format (L"{}", c.charisma)),
                                                   MakeLabel (L"   Wis "),  MakeValue (std::format (L"{}", c.wisdom)),
                                                   MakeLabel (L"   Int "),  MakeValue (std::format (L"{}", c.intelligence)) }));

    for (const auto & [name, count] : c.inventory)
    {
        items += std::format (L"{}{} {}", items.empty() ? L"" : L", ", count, name);
    }

    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Inventory "), MakeValue (items.empty() ? L"none" : items) }));

Error:
    return hr;
}
