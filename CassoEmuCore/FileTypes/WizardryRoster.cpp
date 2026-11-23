#include "Pch.h"

#include "FileTypes/WizardryRoster.h"
#include "FileTypes/WizardryScenario.h"
#include "Core/NumberFormat.h"





static constexpr const wchar_t *  s_kRaces[]    = { L"No race", L"Human", L"Elf", L"Dwarf", L"Gnome", L"Hobbit" };
static constexpr const wchar_t *  s_kClasses[]  = { L"Fighter", L"Mage", L"Priest", L"Thief", L"Bishop", L"Samurai", L"Lord", L"Ninja" };
static constexpr const wchar_t *  s_kAligns[]   = { L"Unaligned", L"Good", L"Neutral", L"Evil" };
static constexpr const wchar_t *  s_kStatuses[] = { L"OK", L"Afraid", L"Asleep", L"Paralyzed", L"Stoned", L"Dead", L"Ashes", L"Lost" };





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::IsRecord
//
//  A name of 1 to 15 characters, and a race, class, alignment and status in
//  the game's ranges.
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryRoster::IsRecord (std::span<const Byte> bytes, size_t at)
{
    int   length = bytes[at + kNameAt];
    bool  fits   = length >= 1 && length <= (int) kShortStringMax;



    fits = fits && WizardryScenario::ReadWord (bytes, at + kRaceAt)   >= 0 && WizardryScenario::ReadWord (bytes, at + kRaceAt)   <= kMaxRace;
    fits = fits && WizardryScenario::ReadWord (bytes, at + kClassAt)  >= 0 && WizardryScenario::ReadWord (bytes, at + kClassAt)  <= kMaxClass;
    fits = fits && WizardryScenario::ReadWord (bytes, at + kAlignAt)  >= 0 && WizardryScenario::ReadWord (bytes, at + kAlignAt)  <= kMaxAlign;
    fits = fits && WizardryScenario::ReadWord (bytes, at + kStatusAt) >= 0 && WizardryScenario::ReadWord (bytes, at + kStatusAt) <= kMaxStatus;

    return fits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::Matches
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryRoster::Matches (const FileFacts & facts, std::span<const Byte> bytes)
{
    return IsPlayerData (facts, bytes) || IsWerdnaData (facts, bytes);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::IsPlayerData
//
//  PLAYER.DATA with at least one record, every record's fields in range.
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryRoster::IsPlayerData (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  named = _stricmp (facts.name.c_str(), "PLAYER.DATA") == 0;
    bool  fits  = named && bytes.size() >= kCharacterBytes;



    for (size_t at = 0; fits && at + kCharacterBytes <= bytes.size(); at += kCharacterBytes)
    {
        fits = IsRecord (bytes, at);
    }

    return fits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::IsWerdnaData
//
//  WERDNA.DATA with a block for each save slot, each block's record a name
//  of 1 to 15 characters.
//
////////////////////////////////////////////////////////////////////////////////

bool WizardryRoster::IsWerdnaData (const FileFacts & facts, std::span<const Byte> bytes)
{
    bool  named = _stricmp (facts.name.c_str(), "WERDNA.DATA") == 0;
    bool  fits  = named && bytes.size() >= kSlots * kSlotBytes;



    for (int slot = 0; fits && slot < kSlots; slot++)
    {
        int  length = bytes[(size_t) slot * kSlotBytes + kNameAt];

        fits = length >= 1 && length <= (int) kShortStringMax;
    }

    return fits;
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::ListSlots
//
//  Each save slot saved to, as a row: its number in bold, then Werdna's
//  level, status, hit points, age in years, gold, keys and item count.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryRoster::ListSlots (std::span<const Byte> bytes, std::vector<Paragraph> & ioParagraphs)
{
    auto  slots  = std::make_shared<RecordTable>();
    int   listed = 0;



    slots->headings = { L"Save slot", L"Level", L"Status", L"Hit points", L"Age", L"Gold", L"Keys", L"Items" };

    for (int slot = 0; slot < kSlots; slot++)
    {
        size_t               at     = (size_t) slot * kSlotBytes;
        int                  level  = WizardryScenario::ReadWord (bytes, at + kWLevelAt);
        int                  status = WizardryScenario::ReadWord (bytes, at + kWStatusAt);
        int                  left   = WizardryScenario::ReadWord (bytes, at + kWHpLeftAt);
        int                  most   = WizardryScenario::ReadWord (bytes, at + kWHpMaxAt);
        RunColor             color  = (status == 0) ? RunColor::Good : RunColor::Bad;
        RunColor             hp     = WizardryScenario::GetHpColor (left, most);
        RecordTable::Record  record;

        if (level < 1)
        {
            continue;
        }

        std::wstring  number = std::format (L"{}", slot + 1);
        std::wstring  shown  = GetStatusName (status);
        std::wstring  hits   = std::format (L"{}/{}", left, most);
        std::wstring  age    = std::format (L"{}", WizardryScenario::ReadWord (bytes, at + kWAgeAt) / kWeeksPerYear);
        std::wstring  gold   = NumberFormat::FormatInteger ((int64_t) WizardryScenario::ReadAmount (bytes, at + kWGoldAt));
        std::wstring  keys   = NumberFormat::FormatInteger ((int64_t) WizardryScenario::ReadAmount (bytes, at + kWKeysAt));
        std::wstring  items  = std::format (L"{}", (std::clamp) (WizardryScenario::ReadWord (bytes, at + kWItemCountAt), 0, 8));

        record.cells = { Run { number, true }, Run { std::format (L"{}", level) }, Run { shown, true, false, false, color },
                         Run { hits, true, false, false, hp }, Run { age }, Run { gold }, Run { keys }, Run { items } };

        record.card.push_back (WizardryScenario::MakeParagraph ({ Run { L"Save slot ", false, false, false, RunColor::Muted },
                                                                  Run { number, true },
                                                                  Run { L"   Level ", false, false, false, RunColor::Muted },
                                                                  Run { std::format (L"{}", level), true, false, false, RunColor::Keyword } }));
        record.card.push_back (WizardryScenario::MakeParagraph ({ Run { L"Status ", false, false, false, RunColor::Muted },
                                                                  Run { shown, true, false, false, color },
                                                                  Run { L"   HP ", false, false, false, RunColor::Muted },
                                                                  Run { hits, true, false, false, hp },
                                                                  Run { L"   Gold ", false, false, false, RunColor::Muted },
                                                                  Run { gold, true, false, false, RunColor::Keyword },
                                                                  Run { L"   Keys ", false, false, false, RunColor::Muted },
                                                                  Run { keys, true, false, false, RunColor::Keyword },
                                                                  Run { L"   Items ", false, false, false, RunColor::Muted },
                                                                  Run { items, true, false, false, RunColor::Keyword } }));

        slots->records.push_back (std::move (record));
        listed++;
    }

    slots->title = std::format (L"Saved game: {} {}", listed, (listed == 1) ? L"save slot" : L"save slots");

    if (listed > 0)
    {
        Paragraph  table;

        table.table = slots;
        ioParagraphs.push_back (std::move (table));
    }
    else
    {
        ioParagraphs.push_back (WizardryScenario::MakeParagraph ({ Run { slots->title, true } }));
        ioParagraphs.push_back (Paragraph());
        ioParagraphs.push_back (WizardryScenario::MakeParagraph ({ Run { L"No game saved yet." } }));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::GetRaceName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryRoster::GetRaceName (int race)
{
    return (race >= 0 && race <= kMaxRace) ? s_kRaces[race] : std::format (L"{}", race);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::GetClassName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryRoster::GetClassName (int kind)
{
    return (kind >= 0 && kind <= kMaxClass) ? s_kClasses[kind] : std::format (L"{}", kind);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::GetAlignName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryRoster::GetAlignName (int align)
{
    return (align >= 0 && align <= kMaxAlign) ? s_kAligns[align] : std::format (L"{}", align);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::GetStatusName
//
////////////////////////////////////////////////////////////////////////////////

std::wstring WizardryRoster::GetStatusName (int status)
{
    return (status >= 0 && status <= kMaxStatus) ? s_kStatuses[status] : std::format (L"{}", status);
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::ListRoster
//
//  Each character with a slot in use, as the roster of Wizardry I, II and
//  III: the name in bold, the level, race, class and alignment, the status
//  in green when OK and red otherwise, the hit points, the age in years,
//  the gold and the experience. With the game's item text, a line under it
//  for each item, by its true name.
//
////////////////////////////////////////////////////////////////////////////////

void WizardryRoster::ListRoster (std::span<const Byte> bytes, const std::vector<WizardryText::ItemName> & items, std::vector<Paragraph> & ioParagraphs)
{
    auto  roster = std::make_shared<RecordTable>();
    int   listed = 0;



    roster->headings = { L"Name", L"Level", L"Race", L"Class", L"Alignment", L"Status", L"Hit points", L"Age", L"Gold", L"Experience" };

    for (size_t at = 0; at + kCharacterBytes <= bytes.size(); at += kCharacterBytes)
    {
        int                  status = WizardryScenario::ReadWord (bytes, at + kStatusAt);
        int                  left   = WizardryScenario::ReadWord (bytes, at + kHpLeftAt);
        int                  most   = WizardryScenario::ReadWord (bytes, at + kHpMaxAt);
        RunColor             color  = (status == 0) ? RunColor::Good : RunColor::Bad;
        RunColor             hp     = WizardryScenario::GetHpColor (left, most);
        RecordTable::Record  record;

        if (status == kStatusLost)
        {
            continue;
        }

        std::wstring  name  = WizardryScenario::ReadString (bytes, at + kNameAt, kShortStringMax);
        std::wstring  level = std::format (L"{}", WizardryScenario::ReadWord (bytes, at + kLevelAt));
        std::wstring  race  = GetRaceName  (WizardryScenario::ReadWord (bytes, at + kRaceAt));
        std::wstring  kind  = GetClassName (WizardryScenario::ReadWord (bytes, at + kClassAt));
        std::wstring  align = GetAlignName (WizardryScenario::ReadWord (bytes, at + kAlignAt));
        std::wstring  shown = GetStatusName (status);
        std::wstring  hits  = std::format (L"{}/{}", left, most);
        std::wstring  age   = std::format (L"{}", WizardryScenario::ReadWord (bytes, at + kAgeAt) / kWeeksPerYear);
        std::wstring  gold  = NumberFormat::FormatInteger ((int64_t) WizardryScenario::ReadAmount (bytes, at + kGoldAt));
        std::wstring  exp   = NumberFormat::FormatInteger ((int64_t) WizardryScenario::ReadAmount (bytes, at + kExperienceAt));

        record.cells = { Run { name, true }, Run { level }, Run { race, false, true, false, RunColor::Muted },
                         Run { kind, false, true, false, RunColor::Muted }, Run { align, false, true, false, RunColor::Muted },
                         Run { shown, true, false, false, color }, Run { hits, true, false, false, hp }, Run { age }, Run { gold }, Run { exp } };

        record.card.push_back (WizardryScenario::MakeParagraph ({ Run { name, true },
                                                                  Run { L"   Level ", false, false, false, RunColor::Muted },
                                                                  Run { level, true, false, false, RunColor::Keyword },
                                                                  Run { std::format (L" {} {}, {}", race, kind, align), false, true, false, RunColor::Muted } }));
        record.card.push_back (WizardryScenario::MakeParagraph ({ Run { L"Status ", false, false, false, RunColor::Muted },
                                                                  Run { shown, true, false, false, color },
                                                                  Run { L"   HP ", false, false, false, RunColor::Muted },
                                                                  Run { hits, true, false, false, hp },
                                                                  Run { L"   Age ", false, false, false, RunColor::Muted },
                                                                  Run { age, true, false, false, RunColor::Keyword },
                                                                  Run { L"   Gold ", false, false, false, RunColor::Muted },
                                                                  Run { gold, true, false, false, RunColor::Keyword },
                                                                  Run { L"   Exp ", false, false, false, RunColor::Muted },
                                                                  Run { exp, true, false, false, RunColor::Keyword } }));

        int  count = items.empty() ? 0 : (std::clamp) (WizardryScenario::ReadWord (bytes, at + kItemCountAt), 0, kMaxItems);

        for (int k = 0; k < count; k++)
        {
            int           item = WizardryScenario::ReadWord (bytes, at + kItemsAt + (size_t) k * kItemBytes);
            std::wstring  text = (item >= 0 && item < (int) items.size()) ? items[(size_t) item].name : std::format (L"Item {}", item);

            record.details.push_back (WizardryScenario::MakeParagraph ({ Run { L"      " + text } }));
        }

        roster->records.push_back (std::move (record));
        listed++;
    }

    roster->title = std::format (L"Saved game: {} {}", listed, (listed == 1) ? L"character" : L"characters");

    if (listed > 0)
    {
        Paragraph  table;

        table.table = roster;
        ioParagraphs.push_back (std::move (table));
    }
    else
    {
        ioParagraphs.push_back (WizardryScenario::MakeParagraph ({ Run { roster->title, true } }));
        ioParagraphs.push_back (Paragraph());
        ioParagraphs.push_back (WizardryScenario::MakeParagraph ({ Run { L"No characters." } }));
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRoster::Decode
//
////////////////////////////////////////////////////////////////////////////////

HRESULT WizardryRoster::Decode (std::span<const Byte> bytes, const FileFacts & facts, const DecodeOptions & options, DecodedFile & outFile, std::wstring & outError)
{
    HRESULT                               hr      = S_OK;
    bool                                  isValid = Matches (facts, bytes);
    std::vector<WizardryText::ItemName>   items;



    outFile        = DecodedFile();
    outFile.format = FileFormat::WizardryRoster;
    outFile.kind   = DecodedFile::Kind::Document;

    CBRFEx (isValid, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA), outError = L"The roster's records can't be read.");

    if (IsWerdnaData (facts, bytes))
    {
        outFile.paragraphs.push_back (WizardryScenario::MakeParagraph ({ Run { L"Wizardry IV: The Return of Werdna", true } }));
        outFile.paragraphs.push_back (Paragraph());
        ListSlots (bytes, outFile.paragraphs);
    }
    else
    {
        outFile.paragraphs.push_back (WizardryScenario::MakeParagraph ({ Run { L"Wizardry V: Heart of the Maelstrom", true } }));
        outFile.paragraphs.push_back (Paragraph());
        //  Without the disk the file came from, the items aren't listed.
        if (!WizardryText::ReadItemNames (options.diskSectors, items))
        {
            items.clear();
        }

        ListRoster (bytes, items, outFile.paragraphs);
    }

Error:
    return hr;
}
