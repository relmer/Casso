#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/WizardryScenario.h"
#include "Core/UnicodeSymbols.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "WizardryScenarioBuilder.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenarioTests
//
//  WizardryScenarioBuilder's SCENARIO.DATA, decoded. The real scenarios are in
//  ScenarioTests/GameDataTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WizardryScenarioTests)
{
public:

    static FileFacts MakeFacts (const char * name)
    {
        FileFacts  facts;



        facts.volume = VolumeKind::Pascal;
        facts.name   = name;

        return facts;
    }


    //  Each paragraph's text, its runs joined.
    static std::vector<std::wstring> GetTexts (const DecodedFile & file)
    {
        std::vector<std::wstring>  texts;

        for (const Paragraph & paragraph : file.paragraphs)
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


    //  The paragraph whose text is `text`, which must be there.
    static const Paragraph & FindParagraph (const DecodedFile & file, const std::wstring & text)
    {
        std::vector<std::wstring>  texts = GetTexts (file);
        auto                       found = std::find (texts.begin(), texts.end(), text);

        Assert::IsTrue (found != texts.end(), text.c_str());

        return file.paragraphs[(size_t) (found - texts.begin())];
    }


    TEST_METHOD (Matches_ByNameAndTableOfContents)
    {
        std::vector<Byte>  bytes  = WizardryScenarioBuilder::MakeScenario();
        std::vector<Byte>  broken = bytes;



        WizardryScenarioBuilder::PutWord (broken, 42 + 2 * WizardryScenario::kZoneMonster, 7);

        Assert::IsTrue  (WizardryScenario::Matches (MakeFacts ("SCENARIO.DATA"), bytes));
        Assert::IsFalse (WizardryScenario::Matches (MakeFacts ("SCENARIO.MESGS"), bytes));
        Assert::IsFalse (WizardryScenario::Matches (MakeFacts ("SCENARIO.DATA"), broken), L"seven monsters don't fit two blocks");
    }


    //  The saved game: a block for each character, with its items under it
    //  by name and marked, and none of the scenario's monsters or items. A
    //  deleted character isn't listed.
    TEST_METHOD (Decode_ListsTheRosterWithEachCharactersItems)
    {
        std::vector<Byte>  bytes = WizardryScenarioBuilder::MakeScenario();
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (WizardryScenario::Decode (bytes, MakeFacts ("SCENARIO.DATA"), DecodeOptions(), file, error));

        Assert::IsTrue   (file.kind == DecodedFile::Kind::Document);
        Assert::AreEqual (std::wstring (L"Wizardry: TEST SCENARIO"), GetTexts (file)[0]);
        Assert::AreEqual (std::wstring (L"Saved game: 1 character"), GetTexts (file)[1]);

        const Paragraph &  gandalf = FindParagraph (file, L"GANDALF   Level 13 Elf Mage, Evil");
        const Paragraph &  sword   = FindParagraph (file, std::wstring (L"      LONG SWORD (") + s_kpszCheckMark + L" equipped)");
        const Paragraph &  armor   = FindParagraph (file, L"      ARMOR (? unidentified)");

        Assert::IsTrue (gandalf.runs[0].bold);
        Assert::IsTrue (HasText (file, L"Status: OK   Hit points: 40/52   Gold: 14,321   Experience: 2,500"));
        Assert::IsTrue (sword.runs[3].color == RunColor::Good, L"equipped is green, with its check");
        Assert::IsTrue (armor.runs[1].italic && armor.runs[1].color == RunColor::Muted, L"one not identified is gray italics, by its unknown name");
        Assert::IsTrue (armor.runs[3].color == RunColor::Muted);
        Assert::IsFalse (HasText (file, std::wstring (s_kpszCheckMark) + L" equipped     "), L"No legend");
        Assert::IsFalse (HasText (file, L"DELETED"));
        Assert::IsFalse (std::any_of (file.paragraphs.begin(), file.paragraphs.end(), [] (const Paragraph & p) { return !p.picture.empty(); }),
                         L"no map, with no character in the maze");
        Assert::IsTrue  (file.notes.empty());
    }


    //  A character's saved square on a level: that level is drawn as a
    //  picture with the party's square marked.
    TEST_METHOD (Decode_DrawsTheLevelThePartyIsOn)
    {
        std::vector<Byte>  bytes     = WizardryScenarioBuilder::MakeScenario();
        size_t             character = WizardryScenarioBuilder::kCharacterBlock * WizardryScenario::kBlockBytes;
        DecodedFile        file;
        std::wstring       error;



        WizardryScenarioBuilder::PutWord (bytes, character + 40,  5);
        WizardryScenarioBuilder::PutWord (bytes, character + 200, 1);
        WizardryScenarioBuilder::PutWord (bytes, character + 202, 0);
        WizardryScenarioBuilder::PutWord (bytes, character + 204, 1);

        AssertSucceeded (WizardryScenario::Decode (bytes, MakeFacts ("SCENARIO.DATA"), DecodeOptions(), file, error));

        auto  map = std::find_if (file.paragraphs.begin(), file.paragraphs.end(), [] (const Paragraph & p) { return !p.picture.empty(); });

        Assert::IsTrue   (HasText (file, L"Maze level 1, the party at 1 east, 0 north"));
        Assert::IsTrue   (map != file.paragraphs.end());
        Assert::AreEqual (map->width, map->height);
        Assert::IsTrue   (FindParagraph (file, L"Status: Dead   Hit points: 40/52   Gold: 14,321   Experience: 2,500").runs[1].color == RunColor::Bad);
    }


    //  Wizardry III adds 1000 to the item number of what a new character buys.
    //  Such a character out in the maze with no square saved gets a note in
    //  place of the map.
    TEST_METHOD (Decode_ItemNumberPast1000_AndAPartyOutWithNoSquare)
    {
        std::vector<Byte>  bytes     = WizardryScenarioBuilder::MakeScenario();
        size_t             character = WizardryScenarioBuilder::kCharacterBlock * WizardryScenario::kBlockBytes;
        DecodedFile        file;
        std::wstring       error;



        WizardryScenarioBuilder::PutWord (bytes, character + 32, 1);
        WizardryScenarioBuilder::PutWord (bytes, character + 66, 1000);

        AssertSucceeded (WizardryScenario::Decode (bytes, MakeFacts ("SCENARIO.DATA"), DecodeOptions(), file, error));

        Assert::IsTrue (HasText (file, std::wstring (L"      LONG SWORD (") + s_kpszCheckMark + L" equipped)"), L"Item 1000 is item 0");
        Assert::IsTrue (HasText (file, L"Status: Out, in the maze   Hit points: 40/52   Gold: 14,321   Experience: 2,500"));
        Assert::IsTrue (HasText (file, L"No map: the party is still in the maze, and no square is saved for it. To get them back, choose Recover in the boot disk's Utilities menu."));
    }


    TEST_METHOD (ToTitleCase)
    {
        Assert::AreEqual (std::wstring (L"Human"),  WizardryScenario::ToTitleCase (L"HUMAN"));
        Assert::AreEqual (std::wstring (L"P-lyze"), WizardryScenario::ToTitleCase (L"P-LYZE"));
        Assert::AreEqual (std::wstring (L"OK"),     WizardryScenario::ToTitleCase (L"OK"));
    }


    //  The square at (x, y)'s center, and a point a few pixels in from its
    //  west side, in a map RenderLevel drew.
    static uint32_t Pixel (const std::vector<uint32_t> & bgra, int width, int x, int y, int dx, int dy)
    {
        constexpr int  kCell   = 24;
        constexpr int  kBorder = 4;

        return bgra[(size_t) (kBorder + (19 - y) * kCell + dy) * (size_t) width + (size_t) (kBorder + x * kCell + dx)];
    }


    TEST_METHOD (RenderLevel_DrawsWallsDoorsStairsFightZonesAndTheParty)
    {
        std::vector<Byte>      bytes  = WizardryScenarioBuilder::MakeScenario();
        std::vector<uint32_t>  bgra;
        int                    width  = 0;
        int                    height = 0;



        WizardryScenario::RenderLevel (std::span<const Byte> (bytes).subspan (WizardryScenarioBuilder::kMazeBlock * WizardryScenario::kBlockBytes, WizardryScenario::kMazeBytes), 5, 5, bgra, width, height);

        Assert::AreEqual (24 * 20 + 9, width);
        Assert::AreEqual (0xFF202020u, Pixel (bgra, width, 0,  0, 0,  12), L"the wall west of (0, 0)");
        Assert::AreEqual (0xFFC0601Au, Pixel (bgra, width, 0,  0, 24, 12), L"the door east of (0, 0), brown in the middle");
        Assert::AreEqual (0xFFCFE0FFu, Pixel (bgra, width, 1,  0, 8,  8),  L"the stairs at (1, 0)");
        Assert::AreEqual (0xFFFBE0E0u, Pixel (bgra, width, 0,  1, 8,  8),  L"the fight zone at (0, 1)");
        Assert::AreEqual (0xFFE0A800u, Pixel (bgra, width, 5,  5, 12, 12), L"the party at (5, 5)");
        Assert::AreEqual (0xFFFFFFFFu, Pixel (bgra, width, 10, 10, 12, 12), L"an empty square");
    }


    TEST_METHOD (GetTitle_TheRomanNumeralAndTheTitle)
    {
        Assert::AreEqual (std::wstring (L"Wizardry I: Proving Grounds of the Mad Overlord"), WizardryScenario::GetTitle (L"PROVING GROUNDS OF THE MAD OVERLORD!"));
        Assert::AreEqual (std::wstring (L"Wizardry II: The Knight of Diamonds"),             WizardryScenario::GetTitle (L"THE KNIGHT OF DIAMONDS"));
        Assert::AreEqual (std::wstring (L"Wizardry III: Legacy of Llylgamyn"),               WizardryScenario::GetTitle (L"THE LEGACY OF LLYLGAMYN"));
        Assert::AreEqual (std::wstring (L"Wizardry: A NEW SCENARIO"),                        WizardryScenario::GetTitle (L"A NEW SCENARIO"));
    }


    TEST_METHOD (Identify_FindsTheScenarioRow)
    {
        std::vector<Byte>  bytes = WizardryScenarioBuilder::MakeScenario();
        FileEntry          entry;



        entry.name = "SCENARIO.DATA";
        entry.type = 5;

        Assert::IsTrue   (FileFormatTable::Identify (entry, VolumeKind::Pascal, bytes) == FileFormat::WizardryScenario);
        Assert::AreEqual (L".Wizardry.rtf", FileFormatTable::GetSuffix (FileFormat::WizardryScenario));
    }
};
