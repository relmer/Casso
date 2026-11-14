#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/WizardryScenario.h"
#include "Core/UnicodeSymbols.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenarioTests
//
//  A SCENARIO.DATA built here in the layout of Wizardry I's: a table of
//  contents, one maze level, two monsters, two items, a character with
//  both items in its inventory and a deleted character. The level has walls around its
//  edge, a door on the east side of square (0, 0), stairs at (1, 0) and a
//  fight zone at (0, 1). The real scenarios are in ScenarioTests/GameDataTests.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (WizardryScenarioTests)
{
public:

    static constexpr int  kMazeBlock      = 2;
    static constexpr int  kMonsterBlock   = 4;
    static constexpr int  kItemBlock      = 6;
    static constexpr int  kCharacterBlock = 8;
    static constexpr int  kBlocks         = 10;


    static void PutWord (std::vector<Byte> & bytes, size_t at, int value)
    {
        bytes[at]     = (Byte) (value & 0xFF);
        bytes[at + 1] = (Byte) ((value >> 8) & 0xFF);
    }


    static void PutString (std::vector<Byte> & bytes, size_t at, const char * text)
    {
        bytes[at] = (Byte) strlen (text);
        memcpy (&bytes[at + 1], text, strlen (text));
    }


    //  Element y of row x of a packed array of `bits`-wide elements.
    static void PutPacked (std::vector<Byte> & bytes, size_t at, size_t rowBytes, int bits, int x, int y, int value)
    {
        int     perWord = 16 / bits;
        size_t  wordAt  = at + (size_t) x * rowBytes + 2 * (size_t) (y / perWord);
        int     shift   = (y % perWord) * bits;
        int     word    = bytes[wordAt] | (bytes[wordAt + 1] << 8);

        word = (word & ~(((1 << bits) - 1) << shift)) | (value << shift);
        PutWord (bytes, wordAt, word);
    }


    static std::vector<Byte> MakeScenario()
    {
        std::vector<Byte>  bytes (kBlocks * WizardryScenario::kBlockBytes, 0);
        const char *       races[]    = { "NO RACE", "HUMAN", "ELF", "DWARF", "GNOME", "HOBBIT" };
        const char *       classes[]  = { "FIGHTER", "MAGE", "PRIEST", "THIEF", "BISHOP", "SAMURAI", "LORD", "NINJA" };
        const char *       statuses[] = { "OK", "AFRAID", "ASLEEP", "P-LYZE", "STONED", "DEAD", "ASHES", "LOST" };
        const char *       aligns[]   = { "UNALIGN", "GOOD", "NEUTRAL", "EVIL" };
        int                perPair[]  = { 6, 1, 6, 6, 13, 4, 2, 1 };
        int                counts[]   = { 1, 1, 2, 0, 2, 2, 0, 1 };
        int                firsts[]   = { 0, kMazeBlock, kMonsterBlock, kItemBlock, kItemBlock, kCharacterBlock, kCharacterBlock, kCharacterBlock };
        size_t             at         = 106;
        size_t             maze       = kMazeBlock      * WizardryScenario::kBlockBytes;
        size_t             monster    = kMonsterBlock   * WizardryScenario::kBlockBytes;
        size_t             item       = kItemBlock      * WizardryScenario::kBlockBytes;
        size_t             character  = kCharacterBlock * WizardryScenario::kBlockBytes;



        PutString (bytes, 0, "TEST SCENARIO");

        for (int zone = 0; zone < 8; zone++)
        {
            PutWord (bytes, 42 + 2 * (size_t) zone, perPair[zone]);
            PutWord (bytes, 58 + 2 * (size_t) zone, counts[zone]);
            PutWord (bytes, 90 + 2 * (size_t) zone, firsts[zone]);
        }

        for (const char * name : races)    { PutString (bytes, at, name); at += 10; }
        for (const char * name : classes)  { PutString (bytes, at, name); at += 10; }
        for (const char * name : statuses) { PutString (bytes, at, name); at += 10; }
        for (const char * name : aligns)   { PutString (bytes, at, name); at += 10; }

        //  Walls around the edge: west of x 0, east of x 19, south of y 0
        //  and north of y 19. A door east of (0, 0), stairs at (1, 0) and a
        //  fight zone at (0, 1).
        for (int i = 0; i < 20; i++)
        {
            PutPacked (bytes, maze + 0,   6, 2, 0,  i,  1);
            PutPacked (bytes, maze + 240, 6, 2, 19, i,  1);
            PutPacked (bytes, maze + 120, 6, 2, i,  0,  1);
            PutPacked (bytes, maze + 360, 6, 2, i,  19, 1);
        }

        PutPacked (bytes, maze + 240, 6, 2, 0, 0, 2);
        PutPacked (bytes, maze + 0,   6, 2, 1, 0, 2);
        PutPacked (bytes, maze + 560, 10, 4, 1, 0, 1);
        PutPacked (bytes, maze + 760, 0, 4, 0, 1, 1);
        PutPacked (bytes, maze + 480, 4, 1, 0, 1, 1);

        PutString (bytes, monster + 0,  "SLIME");
        PutString (bytes, monster + 32, "BUBBLY SLIME");
        PutString (bytes, monster + 48, "BUBBLY SLIMES");
        PutWord   (bytes, monster + 72, 1);
        PutWord   (bytes, monster + 74, 3);
        PutWord   (bytes, monster + 76, 1);
        PutWord   (bytes, monster + 78, 8);
        PutWord   (bytes, monster + 80, 12);
        PutWord   (bytes, monster + 82, 1);
        PutString (bytes, monster + WizardryScenario::kMonsterBytes + 32, "ORC");
        PutWord   (bytes, monster + WizardryScenario::kMonsterBytes + 80, -2);

        PutString (bytes, item + 0,  "LONG SWORD");
        PutString (bytes, item + 16, "SWORD");
        PutWord   (bytes, item + 32, 0);
        PutWord   (bytes, item + 34, 1);
        PutWord   (bytes, item + 36, 1);
        PutWord   (bytes, item + 44, 2500);
        PutWord   (bytes, item + 46, 1);
        PutString (bytes, item + WizardryScenario::kItemBytes + 0,  "ROBES");
        PutString (bytes, item + WizardryScenario::kItemBytes + 16, "ARMOR");

        PutString (bytes, character + 0,   "GANDALF");
        PutWord   (bytes, character + 34,  2);
        PutWord   (bytes, character + 36,  1);
        PutWord   (bytes, character + 40,  0);
        PutWord   (bytes, character + 42,  3);
        PutWord   (bytes, character + 52,  4321);
        PutWord   (bytes, character + 54,  1);
        PutWord   (bytes, character + 58,  2);
        PutWord   (bytes, character + 60,  1);
        PutWord   (bytes, character + 64,  1);
        PutWord   (bytes, character + 66,  0);
        PutWord   (bytes, character + 74,  1);
        PutWord   (bytes, character + 124, 2500);
        PutWord   (bytes, character + 132, 13);
        PutWord   (bytes, character + 134, 40);
        PutWord   (bytes, character + 136, 52);

        //  A slot whose character was deleted has the status LOST.
        PutString (bytes, character + WizardryScenario::kCharacterBytes + 0,  "DELETED");
        PutWord   (bytes, character + WizardryScenario::kCharacterBytes + 40, 7);

        return bytes;
    }


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
        std::vector<Byte>  bytes  = MakeScenario();
        std::vector<Byte>  broken = bytes;



        PutWord (broken, 42 + 2 * WizardryScenario::kZoneMonster, 7);

        Assert::IsTrue  (WizardryScenario::Matches (MakeFacts ("SCENARIO.DATA"), bytes));
        Assert::IsFalse (WizardryScenario::Matches (MakeFacts ("SCENARIO.MESGS"), bytes));
        Assert::IsFalse (WizardryScenario::Matches (MakeFacts ("SCENARIO.DATA"), broken), L"seven monsters don't fit two blocks");
    }


    //  The saved game: a block for each character, with its items under it
    //  by name and marked, and none of the scenario's monsters or items. A
    //  deleted character isn't listed.
    TEST_METHOD (Decode_ListsTheRosterWithEachCharactersItems)
    {
        std::vector<Byte>  bytes = MakeScenario();
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (WizardryScenario::Decode (bytes, MakeFacts ("SCENARIO.DATA"), DecodeOptions(), file, error));

        Assert::IsTrue   (file.kind == DecodedFile::Kind::Document);
        Assert::AreEqual (std::wstring (L"Wizardry: TEST SCENARIO"), GetTexts (file)[0]);
        Assert::AreEqual (std::wstring (L"Saved game: 1 character"), GetTexts (file)[1]);

        const Paragraph &  gandalf = FindParagraph (file, L"GANDALF   Level 13 Elf Mage, Evil");
        const Paragraph &  sword   = FindParagraph (file, std::wstring (L"      ") + s_kpszCheckMark + L" LONG SWORD");
        const Paragraph &  armor   = FindParagraph (file, L"      ? ARMOR");

        Assert::IsTrue (gandalf.runs[0].bold);
        Assert::IsTrue (HasText (file, L"Status: OK   Hit points: 40/52   Gold: 14,321   Experience: 2,500"));
        Assert::IsTrue (sword.runs[1].color == RunColor::Good, L"an equipped item's check is green");
        Assert::IsTrue (armor.runs[2].italic && armor.runs[2].color == RunColor::Muted, L"one not identified is gray italics, by its unknown name");
        Assert::IsFalse (HasText (file, L"DELETED"));
        Assert::IsFalse (std::any_of (file.paragraphs.begin(), file.paragraphs.end(), [] (const Paragraph & p) { return !p.picture.empty(); }),
                         L"no map, with no character in the maze");
        Assert::IsTrue  (file.notes.empty());
    }


    //  A character's saved square on a level: that level is drawn as a
    //  picture with the party's square marked.
    TEST_METHOD (Decode_DrawsTheLevelThePartyIsOn)
    {
        std::vector<Byte>  bytes     = MakeScenario();
        size_t             character = kCharacterBlock * WizardryScenario::kBlockBytes;
        DecodedFile        file;
        std::wstring       error;



        PutWord (bytes, character + 40,  5);
        PutWord (bytes, character + 200, 1);
        PutWord (bytes, character + 202, 0);
        PutWord (bytes, character + 204, 1);

        AssertSucceeded (WizardryScenario::Decode (bytes, MakeFacts ("SCENARIO.DATA"), DecodeOptions(), file, error));

        auto  map = std::find_if (file.paragraphs.begin(), file.paragraphs.end(), [] (const Paragraph & p) { return !p.picture.empty(); });

        Assert::IsTrue   (HasText (file, L"Maze level 1, the party at 1 east, 0 north"));
        Assert::IsTrue   (map != file.paragraphs.end());
        Assert::AreEqual (map->width, map->height);
        Assert::IsTrue   (FindParagraph (file, L"Status: Dead   Hit points: 40/52   Gold: 14,321   Experience: 2,500").runs[1].color == RunColor::Bad);
    }


    TEST_METHOD (ToTitleCaseAndGroupDigits)
    {
        Assert::AreEqual (std::wstring (L"Human"),  WizardryScenario::ToTitleCase (L"HUMAN"));
        Assert::AreEqual (std::wstring (L"P-lyze"), WizardryScenario::ToTitleCase (L"P-LYZE"));
        Assert::AreEqual (std::wstring (L"OK"),     WizardryScenario::ToTitleCase (L"OK"));
        Assert::AreEqual (std::wstring (L"7,811,260"), WizardryScenario::GroupDigits (7811260));
        Assert::AreEqual (std::wstring (L"441"),       WizardryScenario::GroupDigits (441));
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
        std::vector<Byte>      bytes  = MakeScenario();
        std::vector<uint32_t>  bgra;
        int                    width  = 0;
        int                    height = 0;



        WizardryScenario::RenderLevel (std::span<const Byte> (bytes).subspan (kMazeBlock * WizardryScenario::kBlockBytes, WizardryScenario::kMazeBytes), 5, 5, bgra, width, height);

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
        std::vector<Byte>  bytes = MakeScenario();
        FileEntry          entry;



        entry.name = "SCENARIO.DATA";
        entry.type = 5;

        Assert::IsTrue   (FileFormatTable::Identify (entry, VolumeKind::Pascal, bytes) == FileFormat::WizardryScenario);
        Assert::AreEqual (L".Wizardry.rtf", FileFormatTable::GetSuffix (FileFormat::WizardryScenario));
    }
};
