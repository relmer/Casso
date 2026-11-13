#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/TextEncoding.h"
#include "FileTypes/FileFormatTable.h"
#include "FileTypes/WizardryScenario.h"
#include "Machines/Apple2/Common/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  WizardryScenarioTests
//
//  A SCENARIO.DATA built here in the layout of Wizardry I's: a table of
//  contents, one maze level, two monsters, an item and a character. The
//  level has walls around its edge, a door on the east side of square
//  (0, 0), stairs at (1, 0) and a fight zone at (0, 1). The real scenarios
//  are in ScenarioTests/WizardryTests.
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
        int                counts[]   = { 1, 1, 2, 0, 1, 1, 0, 1 };
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

        PutString (bytes, character + 0,   "GANDALF");
        PutWord   (bytes, character + 34,  2);
        PutWord   (bytes, character + 36,  1);
        PutWord   (bytes, character + 40,  0);
        PutWord   (bytes, character + 42,  3);
        PutWord   (bytes, character + 132, 13);
        PutWord   (bytes, character + 134, 40);
        PutWord   (bytes, character + 136, 52);

        return bytes;
    }


    static FileFacts MakeFacts (const char * name)
    {
        FileFacts  facts;



        facts.volume = VolumeKind::Pascal;
        facts.name   = name;

        return facts;
    }


    static bool HasLine (const DecodedFile & file, const std::wstring & text)
    {
        return std::find (file.lines.begin(), file.lines.end(), text) != file.lines.end();
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


    TEST_METHOD (Decode_ListsMonstersItemsAndCharacters)
    {
        std::vector<Byte>  bytes = MakeScenario();
        DecodedFile        file;
        std::wstring       error;



        AssertSucceeded (WizardryScenario::Decode (bytes, MakeFacts ("SCENARIO.DATA"), DecodeOptions(), file, error));

        Assert::IsTrue   (file.kind == DecodedFile::Kind::Lines);
        Assert::AreEqual (std::wstring (L"TEST SCENARIO"), file.lines[0]);
        Assert::AreEqual (std::wstring (L"Wizardry scenario: 1 maze levels, 2 monsters, 1 items, 1 characters"), file.lines[1]);
        Assert::IsTrue   (HasLine (file, L"   0  BUBBLY SLIME     BUBBLY SLIMES    SLIME            1d3+1       12      8        1"));
        Assert::IsTrue   (HasLine (file, L"   1  ORC                                                0d0+0       -2      0        0"));
        Assert::IsTrue   (HasLine (file, L"   0  LONG SWORD       SWORD            Weapon         12500  GOOD      Yes"));
        Assert::IsTrue   (HasLine (file, L"   0  GANDALF          ELF       MAGE         13      40/52  OK       EVIL"));
        Assert::IsTrue   (file.notes.empty());
    }


    TEST_METHOD (DrawLevel_ShowsWallsDoorsStairsAndFightZones)
    {
        std::vector<Byte>          bytes = MakeScenario();
        std::vector<std::wstring>  map;



        WizardryScenario::DrawLevel (std::span<const Byte> (bytes).subspan (kMazeBlock * WizardryScenario::kBlockBytes, WizardryScenario::kMazeBytes), map);

        Assert::AreEqual ((size_t) 41, map.size());
        Assert::IsTrue   (map[0].starts_with (L"+--+--+"));
        Assert::IsTrue   (map[37].starts_with (L"|*    "), map[37].c_str());
        Assert::IsTrue   (map[39].starts_with (L"|  DSt "), map[39].c_str());
        Assert::IsTrue   (map[39].ends_with (L"   |"));
        Assert::IsTrue   (map[40].starts_with (L"+--+--+"));
    }


    TEST_METHOD (Identify_FindsTheScenarioRow)
    {
        std::vector<Byte>  bytes = MakeScenario();
        FileEntry          entry;



        entry.name = "SCENARIO.DATA";
        entry.type = 5;

        Assert::IsTrue   (FileFormatTable::Identify (entry, VolumeKind::Pascal, bytes) == FileFormat::WizardryScenario);
        Assert::AreEqual (L".Wizardry.txt", FileFormatTable::GetSuffix (FileFormat::WizardryScenario));
    }
};
