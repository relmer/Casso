#include "Pch.h"

#include "Machines/Apple2/Common/Ultima1Save.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Machines/Apple2/Common/Ultima1Map.h"





//  The inventory's counts, a byte each, by name: armor from $11, weapons
//  from $17, magic from $27 and transport from $32. The readied weapon at
//  $0D and armor at $0E are indexes into the same lists from 1. Each
//  offset was found by changing the byte in the game's memory and reading
//  its inventory screen.
static constexpr const wchar_t * s_kArmor[]     = { L"Leather armor", L"Chain mail", L"Plate mail", L"Vacuum suit", L"Reflect suit" };
static constexpr const wchar_t * s_kWeapons[]   = { L"Dagger", L"Mace", L"Axe", L"Rope and spikes", L"Sword", L"Great sword",
                                                    L"Bow and arrows", L"Amulet", L"Wand", L"Staff", L"Triangle", L"Pistol",
                                                    L"Light sword", L"Phazor", L"Blaster" };
static constexpr const wchar_t * s_kSpells[]    = { L"Open", L"Unlock", L"Magic missile", L"Steal", L"Ladder down", L"Ladder up",
                                                    L"Blink", L"Create", L"Destroy", L"Kill" };
static constexpr const wchar_t * s_kTransport[] = { L"Horse", L"Cart", L"Raft", L"Frigate", L"Aircar", L"Shuttle", L"Phantom",
                                                    L"Star cruiser", L"Battle base", L"Time machine" };





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::ReadWord
//
////////////////////////////////////////////////////////////////////////////////

int Ultima1Save::ReadWord (std::span<const Byte> bytes, size_t at)
{
    constexpr int  kByteBits = 8;



    return bytes[at] | (bytes[at + 1] << kByteBits);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::AddCounts
//
//  Each name whose count, a byte from `at` on, isn't zero.
//
////////////////////////////////////////////////////////////////////////////////

void Ultima1Save::AddCounts (std::span<const Byte> vars, size_t at, std::span<const wchar_t * const> names, Character & ioCharacter)
{
    for (size_t i = 0; i < names.size(); i++)
    {
        if (vars[at + i] != 0)
        {
            ioCharacter.inventory.emplace_back (names[i], vars[at + i]);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::TryReadVars
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1Save::TryReadVars (const std::vector<Byte> & sectors, std::vector<Byte> & outVars)
{
    HRESULT       hr      = S_OK;
    ProDosVolume  volume (sectors);
    FilePayload   vars;
    bool          isSized = false;



    hr = volume.Read (FilePath::Parse ("U1.VARS"), vars);
    CHR (hr);

    isSized = vars.bytes.size() >= kVarsBytes;
    CBR (isSized);

    outVars = std::move (vars.bytes);

Error:
    return SUCCEEDED (hr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::TryParse
//
//  The position at $06, a byte each for column and row; the sex at $4A,
//  1 for female; the name from $4B, up to 14 letters ending at a zero; then
//  little-endian words from $59: hit points, Str, Agi, Sta, Cha, Wis, Int,
//  gold, the race from 1 and the class from 1; and food, a word at $78.
//  False for a race or class out of range.
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1Save::TryParse (std::span<const Byte> vars, Character & outCharacter)
{
    constexpr size_t  kXAt        = 0x06;
    constexpr size_t  kYAt        = 0x07;
    constexpr size_t  kSexAt      = 0x4A;
    constexpr size_t  kNameAt     = 0x4B;
    constexpr size_t  kNameBytes  = 14;
    constexpr size_t  kHitsAt     = 0x59;
    constexpr size_t  kGoldAt     = 0x67;
    constexpr size_t  kRaceAt     = 0x69;
    constexpr size_t  kClassAt    = 0x6B;
    constexpr size_t  kFoodAt     = 0x78;
    constexpr size_t  kLandAt     = 0x02;
    constexpr Byte    kLandMask   = 0x03;
    constexpr size_t  kWeaponAt   = 0x0D;
    constexpr size_t  kArmorAt    = 0x0E;
    constexpr size_t  kArmorsAt   = 0x11;
    constexpr size_t  kWeaponsAt  = 0x17;
    constexpr size_t  kSpellsAt   = 0x27;
    constexpr size_t  kVehiclesAt = 0x32;
    constexpr size_t  kWordBytes  = 2;
    constexpr int     kKinds      = 4;
    constexpr Byte    kFirstGlyph = 0x20;
    int               race        = 0;
    int               kind        = 0;



    if (vars.size() < kVarsBytes)
    {
        return false;
    }

    race = ReadWord (vars, kRaceAt);
    kind = ReadWord (vars, kClassAt);

    if (race < 1 || race > kKinds || kind < 1 || kind > kKinds || vars[kNameAt] < kFirstGlyph)
    {
        return false;
    }

    outCharacter = Character();

    for (size_t i = 0; i < kNameBytes && vars[kNameAt + i] >= kFirstGlyph; i++)
    {
        outCharacter.name += (wchar_t) vars[kNameAt + i];
    }

    outCharacter.x              = vars[kXAt];
    outCharacter.y              = vars[kYAt];
    outCharacter.isFemale       = vars[kSexAt] == 1;
    outCharacter.race           = race;
    outCharacter.characterClass = kind;
    outCharacter.hitPoints      = ReadWord (vars, kHitsAt);
    outCharacter.strength       = ReadWord (vars, kHitsAt + kWordBytes);
    outCharacter.agility        = ReadWord (vars, kHitsAt + kWordBytes * 2);
    outCharacter.stamina        = ReadWord (vars, kHitsAt + kWordBytes * 3);
    outCharacter.charisma       = ReadWord (vars, kHitsAt + kWordBytes * 4);
    outCharacter.wisdom         = ReadWord (vars, kHitsAt + kWordBytes * 5);
    outCharacter.intelligence   = ReadWord (vars, kHitsAt + kWordBytes * 6);
    outCharacter.gold           = ReadWord (vars, kGoldAt);
    outCharacter.food           = ReadWord (vars, kFoodAt);
    outCharacter.continent      = vars[kLandAt] & kLandMask;

    if (vars[kWeaponAt] >= 1 && vars[kWeaponAt] <= std::size (s_kWeapons))
    {
        outCharacter.weapon = s_kWeapons[vars[kWeaponAt] - 1];
    }

    if (vars[kArmorAt] >= 1 && vars[kArmorAt] <= std::size (s_kArmor))
    {
        outCharacter.armor = s_kArmor[vars[kArmorAt] - 1];
    }

    AddCounts (vars, kArmorsAt,   s_kArmor,     outCharacter);
    AddCounts (vars, kWeaponsAt,  s_kWeapons,   outCharacter);
    AddCounts (vars, kSpellsAt,   s_kSpells,    outCharacter);
    AddCounts (vars, kVehiclesAt, s_kTransport, outCharacter);

    return true;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::IsFound
//
////////////////////////////////////////////////////////////////////////////////

bool Ultima1Save::IsFound (const std::vector<Byte> & sectors)
{
    std::vector<Byte>  vars;
    Character          character;



    return TryReadVars (sectors, vars) && TryParse (vars, character);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::GetRaceName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima1Save::GetRaceName (int race)
{
    switch (race)
    {
        case 1:  return L"Human";
        case 2:  return L"Elf";
        case 3:  return L"Dwarf";
        case 4:  return L"Bobbit";
        default: return std::format (L"Race {}", race);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::GetClassName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring Ultima1Save::GetClassName (int characterClass)
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
//  Ultima1Save::MakeParagraph
//
////////////////////////////////////////////////////////////////////////////////

Paragraph Ultima1Save::MakeParagraph (std::vector<Run> runs)
{
    Paragraph  paragraph;



    paragraph.runs = std::move (runs);

    return paragraph;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::MakeLabel
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima1Save::MakeLabel (const std::wstring & text)
{
    return Run { text, false, false, false, RunColor::Muted };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::MakeValue
//
////////////////////////////////////////////////////////////////////////////////

Run Ultima1Save::MakeValue (const std::wstring & text)
{
    return Run { text, true, false, false, RunColor::Keyword };
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::Decode
//
//  The saved game without its map, which is on the game disk.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ultima1Save::Decode (const std::vector<Byte> & sectors, std::span<const bool> unreadable, DecodedFile & outFile, std::wstring & outError)
{
    UNREFERENCED_PARAMETER (unreadable);

    return DecodeWithMap (sectors, nullptr, outFile, outError);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Ultima1Save::DecodeWithMap
//
//  The title and position, the player's continent with the player marked
//  when the game disk is given, then the character: name, race, class and
//  sex, hit points, food and gold, the six stats, the readied weapon and
//  armor, and the inventory.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT Ultima1Save::DecodeWithMap (const std::vector<Byte> & sectors, const std::vector<Byte> * gameSectors, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT                                                     hr       = S_OK;
    std::vector<Byte>                                           vars;
    Character                                                   c;
    std::wstring                                                items;
    bool                                                        isParsed = false;
    std::array<Ultima1Map::Continent, Ultima1Map::kContinents>  continents;



    outFile        = DecodedFile();
    outFile.format = FileFormat::UltimaSave;
    outFile.kind   = DecodedFile::Kind::Document;

    isParsed = TryReadVars (sectors, vars) && TryParse (vars, c);
    CBRFEx (isParsed, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The disk doesn't have an Ultima I saved game.");

    outFile.paragraphs.push_back (MakeParagraph ({ Run { L"Ultima I: The Beginning", true } }));
    outFile.paragraphs.push_back (MakeParagraph ({ Run { std::format (L"Saved game, the player in {} at column {}, row {}", Ultima1Map::GetContinentName (c.continent), c.x, c.y), false, false, false, RunColor::Muted } }));

    if (gameSectors != nullptr && Ultima1Map::TryReadGameDisk (*gameSectors, continents))
    {
        Paragraph  map;

        Ultima1Map::RenderMap (continents[(size_t) c.continent], c.x, c.y, map);
        outFile.paragraphs.push_back (std::move (map));
        Ultima1Map::AddLegend (outFile.paragraphs, true);
    }

    outFile.paragraphs.push_back (Paragraph());
    outFile.paragraphs.push_back (MakeParagraph ({ Run { c.name, true },
                                                   Run { std::format (L"   {} {}, {}", GetRaceName (c.race), GetClassName (c.characterClass), c.isFemale ? L"female" : L"male"), false, true, false, RunColor::Muted } }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"HP "),      MakeValue (std::format (L"{}", c.hitPoints)),
                                                   MakeLabel (L"   Food "), MakeValue (std::format (L"{}", c.food)),
                                                   MakeLabel (L"   Coin "), MakeValue (std::format (L"{}", c.gold)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Str "),     MakeValue (std::format (L"{}", c.strength)),
                                                   MakeLabel (L"   Agi "),  MakeValue (std::format (L"{}", c.agility)),
                                                   MakeLabel (L"   Sta "),  MakeValue (std::format (L"{}", c.stamina)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Cha "),     MakeValue (std::format (L"{}", c.charisma)),
                                                   MakeLabel (L"   Wis "),  MakeValue (std::format (L"{}", c.wisdom)),
                                                   MakeLabel (L"   Int "),  MakeValue (std::format (L"{}", c.intelligence)) }));
    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Weapon "),   MakeValue (c.weapon.empty() ? L"hands" : c.weapon),
                                                   MakeLabel (L"   Armor "), MakeValue (c.armor.empty()  ? L"skin"  : c.armor) }));

    for (const auto & [name, count] : c.inventory)
    {
        items += std::format (L"{}{} {}", items.empty() ? L"" : L", ", count, name);
    }

    outFile.paragraphs.push_back (MakeParagraph ({ MakeLabel (L"Inventory "), MakeValue (items.empty() ? L"none" : items) }));

Error:
    return hr;
}
