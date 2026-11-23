#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/WizardryRoster.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryRosterTests
//
//  Wizardry V's PLAYER.DATA, built here, decoded. The real file is in
//  ScenarioTests/GameDataTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WizardryRosterTests)
{
public:

    static constexpr int  kSlots = 4;


    static FileFacts MakeFacts (const char * name)
    {
        FileFacts  facts;



        facts.volume = VolumeKind::Pascal;
        facts.name   = name;

        return facts;
    }


    static void PutWord (std::vector<Byte> & bytes, size_t at, int value)
    {
        bytes[at]     = (Byte) (value & 0xFF);
        bytes[at + 1] = (Byte) ((value >> 8) & 0xFF);
    }


    static void PutName (std::vector<Byte> & bytes, size_t at, const char * name)
    {
        size_t  length = strlen (name);



        bytes[at] = (Byte) length;
        memcpy (&bytes[at + 1], name, length);
    }


    //  Four slots: a dwarf fighter in the first, a dead elf mage in the
    //  third, and the other two not in use, as the game writes them.
    static std::vector<Byte> MakeRoster()
    {
        std::vector<Byte>  bytes (kSlots * WizardryRoster::kCharacterBytes, 0);



        for (int i = 0; i < kSlots; i++)
        {
            size_t  at = (size_t) i * WizardryRoster::kCharacterBytes;

            PutName (bytes, at + WizardryRoster::kNameAt, "UNSET");
            PutWord (bytes, at + WizardryRoster::kRaceAt,   1);
            PutWord (bytes, at + WizardryRoster::kAlignAt,  2);
            PutWord (bytes, at + WizardryRoster::kStatusAt, WizardryRoster::kStatusLost);
        }

        PutName (bytes, WizardryRoster::kNameAt, "ALDRIC");
        PutWord (bytes, WizardryRoster::kRaceAt,             3);
        PutWord (bytes, WizardryRoster::kClassAt,            0);
        PutWord (bytes, WizardryRoster::kAlignAt,            1);
        PutWord (bytes, WizardryRoster::kStatusAt,           0);
        PutWord (bytes, WizardryRoster::kAgeAt,              1042);
        PutWord (bytes, WizardryRoster::kGoldAt,             2345);
        PutWord (bytes, WizardryRoster::kGoldAt + 2,         1);
        PutWord (bytes, WizardryRoster::kExperienceAt,       500);
        PutWord (bytes, WizardryRoster::kLevelAt,            3);
        PutWord (bytes, WizardryRoster::kHpLeftAt,           20);
        PutWord (bytes, WizardryRoster::kHpMaxAt,            20);

        size_t  mage = 2 * WizardryRoster::kCharacterBytes;

        PutName (bytes, mage + WizardryRoster::kNameAt, "MIRIEL");
        PutWord (bytes, mage + WizardryRoster::kRaceAt,      2);
        PutWord (bytes, mage + WizardryRoster::kClassAt,     1);
        PutWord (bytes, mage + WizardryRoster::kStatusAt,    5);
        PutWord (bytes, mage + WizardryRoster::kAgeAt,       1300);
        PutWord (bytes, mage + WizardryRoster::kLevelAt,     2);
        PutWord (bytes, mage + WizardryRoster::kHpMaxAt,     6);

        return bytes;
    }


    //  Each paragraph's text, its runs joined, the roster's table as its
    //  cards.
    static std::vector<std::wstring> GetTexts (const DecodedFile & file)
    {
        std::vector<std::wstring>  texts;

        for (const Paragraph & paragraph : FlattenRecordTables (file.paragraphs))
        {
            std::wstring  text;

            for (const Run & run : paragraph.runs)
            {
                text += run.text;
            }

            texts.push_back (text);
        }

        return texts;
    }


    static bool HasText (const DecodedFile & file, const std::wstring & text)
    {
        std::vector<std::wstring>  texts = GetTexts (file);

        return std::find (texts.begin(), texts.end(), text) != texts.end();
    }


    //  Wizardry IV's WERDNA.DATA: Werdna in each of the eight slots, at level
    //  0 in a slot not saved to. Slots 1 and 3 are saved here.
    TEST_METHOD (Decode_WerdnaData_ListsTheSlotsSavedTo)
    {
        std::vector<Byte>  bytes (WizardryRoster::kSlots * WizardryRoster::kSlotBytes, 0);
        DecodedFile        file;
        std::wstring       error;



        for (int slot = 0; slot < WizardryRoster::kSlots; slot++)
        {
            size_t  at = (size_t) slot * WizardryRoster::kSlotBytes;

            PutName (bytes, at, "WERDNA");
            PutWord (bytes, at + WizardryRoster::kWAgeAt,   5200);
            PutWord (bytes, at + WizardryRoster::kWHpLeftAt, 1);
            PutWord (bytes, at + WizardryRoster::kWHpMaxAt,  1);
        }

        PutWord (bytes, WizardryRoster::kWLevelAt,     1);
        PutWord (bytes, WizardryRoster::kWHpLeftAt,    10);
        PutWord (bytes, WizardryRoster::kWHpMaxAt,     10);
        PutWord (bytes, WizardryRoster::kWGoldAt,      879);
        PutWord (bytes, WizardryRoster::kWKeysAt,      9845);
        PutWord (bytes, WizardryRoster::kWKeysAt + 2,  99);
        PutWord (bytes, WizardryRoster::kWItemCountAt, 4);
        PutWord (bytes, 2 * WizardryRoster::kSlotBytes + WizardryRoster::kWLevelAt, 2);

        Assert::IsTrue  (WizardryRoster::Matches (MakeFacts ("WERDNA.DATA"), bytes));
        AssertSucceeded (WizardryRoster::Decode (bytes, MakeFacts ("WERDNA.DATA"), DecodeOptions(), file, error));

        Assert::AreEqual (std::wstring (L"Wizardry IV: The Return of Werdna"), GetTexts (file)[0]);
        Assert::IsTrue   (HasText (file, L"Saved game: 2 save slots"));
        Assert::IsTrue   (HasText (file, L"Save slot 1   Level 1"));
        Assert::IsTrue   (HasText (file, L"Status OK   HP 10/10   Gold 879   Keys 999,845   Items 4"));
        Assert::IsTrue   (HasText (file, L"Save slot 3   Level 2"));
        Assert::IsFalse  (HasText (file, L"Save slot 2   Level 0"));
    }


    TEST_METHOD (Matches_ByNameAndRecords)
    {
        std::vector<Byte>  bytes  = MakeRoster();
        std::vector<Byte>  broken = bytes;



        PutWord (broken, WizardryRoster::kCharacterBytes + WizardryRoster::kClassAt, 12);

        Assert::IsTrue  (WizardryRoster::Matches (MakeFacts ("PLAYER.DATA"), bytes));
        Assert::IsFalse (WizardryRoster::Matches (MakeFacts ("PLAYER.INFO"), bytes));
        Assert::IsFalse (WizardryRoster::Matches (MakeFacts ("PLAYER.DATA"), broken), L"no twelfth class");
        Assert::IsTrue  (FileFormatTable::Identify (FileEntry { .name = "PLAYER.DATA" }, VolumeKind::Pascal, bytes) == FileFormat::WizardryRoster);
    }


    //  The characters in use, with the game's labels, and the dead one's
    //  status in red. Slots not in use aren't listed.
    TEST_METHOD (Decode_ListsTheCharactersInUse)
    {
        std::vector<Byte>  bytes = MakeRoster();
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (WizardryRoster::Decode (bytes, MakeFacts ("PLAYER.DATA"), DecodeOptions(), file, error));

        Assert::IsTrue   (file.kind == DecodedFile::Kind::Document);
        Assert::AreEqual (std::wstring (L"Wizardry V: Heart of the Maelstrom"), GetTexts (file)[0]);
        Assert::IsTrue   (HasText (file, L"Saved game: 2 characters"));
        Assert::IsTrue   (HasText (file, L"ALDRIC   Level 3 Dwarf Fighter, Good"));
        Assert::IsTrue   (HasText (file, L"Status OK   HP 20/20   Age 20   Gold 12,345   Exp 500"));
        Assert::IsTrue   (HasText (file, L"MIRIEL   Level 2 Elf Mage, Neutral"));
        Assert::IsTrue   (HasText (file, L"Status Dead   HP 0/6   Age 25   Gold 0   Exp 0"));
        Assert::IsFalse  (HasText (file, L"UNSET"));
    }
};
