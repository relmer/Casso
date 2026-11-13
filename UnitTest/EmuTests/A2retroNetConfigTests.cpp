#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Devices/Disk/A2retroNetConfig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  A2retroNetConfigTests
//
//  A2retroNET.txt read and written as the A2retroNET firmware's config.c
//  reads and writes it, and disk images checked as its hdd.c opens them.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (A2retroNetConfigTests)
{
public:

    static std::vector<Byte> MakeTwoImg (size_t blocks, Byte format, Word offset)
    {
        std::vector<Byte>  bytes (64 + blocks * 512, 0);



        memcpy (bytes.data(), "2IMG", 4);
        bytes[0x0C] = format;
        bytes[0x18] = (Byte) (offset & 0xFF);
        bytes[0x19] = (Byte) (offset >> 8);

        return bytes;
    }


    //  The README's example, with a comment, CR LF line ends and settings out
    //  of range, which the firmware ignores.
    TEST_METHOD (Parse_ReadsTheSettingsAndDrives)
    {
        A2retroNetConfig  config = A2retroNetConfig::Parse ("[settings]\r\nbootdelay=5\r\n[drives]\r\nnumber=4\r\n1=system.hdv\r\n#2=old.po\r\n2=work.hdv\r\n3=sd:/games/zork.po\r\n9=ignored.po\r\n");
        A2retroNetConfig  odd    = A2retroNetConfig::Parse ("bootdelay=7\n[drives]\nnumber=3\n1=a.po\n[other]\n2=b.po\n");



        Assert::AreEqual (5, config.bootDelay);
        Assert::AreEqual (4, config.number);
        Assert::AreEqual (std::string ("system.hdv"),        config.paths[0]);
        Assert::AreEqual (std::string ("work.hdv"),          config.paths[1]);
        Assert::AreEqual (std::string ("sd:/games/zork.po"), config.paths[2]);
        Assert::IsTrue   (config.paths[3].empty());

        Assert::AreEqual (A2retroNetConfig::kBootDelayDefault, odd.bootDelay, L"outside [settings]");
        Assert::AreEqual (A2retroNetConfig::kDrivesMax,        odd.number,    L"an odd number");
        Assert::AreEqual (std::string ("a.po"), odd.paths[0]);
        Assert::IsTrue   (odd.paths[1].empty(), L"outside [drives]");
    }


    TEST_METHOD (Write_IsWhatTheFirmwareWrites)
    {
        A2retroNetConfig  config;
        int               free = -1;



        config.number   = 4;
        config.paths[0] = "system.hdv";
        config.paths[2] = "games/zork.po";

        Assert::AreEqual (std::string ("[settings]\nbootdelay=3\n[drives]\nnumber=4\n1=system.hdv\n2=\n3=games/zork.po\n4=\n"), config.Write());

        Assert::IsTrue   (config.TryFindFreeDrive (free));
        Assert::AreEqual (1, free);

        config.paths[1] = "x.po";
        config.paths[3] = "y.po";
        Assert::IsFalse  (config.TryFindFreeDrive (free), L"drives 5 to 8 aren't in use");
    }


    //  The README suggests commenting out a drive's other images to switch
    //  between them, so a change rewrites only the lines it sets.
    TEST_METHOD (Write_ChangesOnlyTheLinesItSets)
    {
        std::string       text   = "# my card\r\n[settings]\r\nbootdelay=5\r\n[drives]\r\nnumber=4\r\n1=system.hdv\r\n#2=old.po\r\n2=work.hdv\r\n#3=games/a.po\r\n";
        A2retroNetConfig  config = A2retroNetConfig::Parse (text);



        Assert::AreEqual (std::string ("# my card\n[settings]\nbootdelay=5\n[drives]\nnumber=4\n1=system.hdv\n#2=old.po\n2=work.hdv\n#3=games/a.po\n"), config.Write(), L"the same text without CRs");

        config.bootDelay = 2;
        config.paths[1]  = "new.po";
        config.paths[2]  = "games/c.po";

        Assert::AreEqual (std::string ("# my card\n[settings]\nbootdelay=2\n[drives]\nnumber=4\n1=system.hdv\n#2=old.po\n2=new.po\n#3=games/a.po\n3=games/c.po\n"), config.Write());
    }


    TEST_METHOD (Write_AddsAMissingLineToItsSection)
    {
        A2retroNetConfig  config = A2retroNetConfig::Parse ("[drives]\n1=a.po\n[other]\nx=1\n");
        A2retroNetConfig  bare   = A2retroNetConfig::Parse ("# drives only\n[drives]\n1=a.po\n");



        config.paths[1] = "b.po";
        Assert::AreEqual (std::string ("[drives]\n1=a.po\n2=b.po\n[other]\nx=1\n"), config.Write(), L"defaults aren't added");

        bare.bootDelay = 7;
        bare.number    = 2;
        Assert::AreEqual (std::string ("[settings]\nbootdelay=7\n# drives only\n[drives]\nnumber=2\n1=a.po\n"), bare.Write());
    }


    TEST_METHOD (Alternatives_AreADrivesCommentedOutImages)
    {
        A2retroNetConfig          config = A2retroNetConfig::Parse ("[drives]\n#3=games/a.po\n#3=games/b.po\n3=games/x.po\n#4=empty.po\n");
        std::vector<std::string>  alternatives;



        alternatives = config.GetAlternatives (2);
        Assert::AreEqual ((size_t) 2, alternatives.size());
        Assert::AreEqual (std::string ("games/a.po"), alternatives[0]);
        Assert::AreEqual (std::string ("games/b.po"), alternatives[1]);

        Assert::IsTrue  (config.SwitchToAlternative (2, "games/b.po"));
        Assert::AreEqual (std::string ("games/b.po"), config.paths[2]);
        Assert::AreEqual (std::string ("[drives]\n#3=games/a.po\n#3=games/x.po\n3=games/b.po\n#4=empty.po\n"), config.Write(), L"the image switched from is commented out in its place");

        Assert::IsTrue  (config.SwitchToAlternative (3, "empty.po"));
        Assert::AreEqual (std::string ("[drives]\n#3=games/a.po\n#3=games/x.po\n3=games/b.po\n4=empty.po\n"), config.Write(), L"a drive with no image has nothing to comment out");

        Assert::IsFalse (config.SwitchToAlternative (2, "games/none.po"));
    }


    TEST_METHOD (CheckImage_TakesWhatTheFirmwareReads)
    {
        std::vector<Byte>  blocks (280 * 512, 0);
        std::vector<Byte>  huge   (65536ull * 512, 0);
        std::string        problem;



        Assert::IsTrue  (A2retroNetConfig::CheckImage ("game.po",   blocks, problem));
        Assert::IsTrue  (A2retroNetConfig::CheckImage ("WORK.HDV",  blocks, problem));
        Assert::IsTrue  (A2retroNetConfig::CheckImage ("disk.2mg",  MakeTwoImg (1600, 1, 64), problem));

        Assert::IsFalse (A2retroNetConfig::CheckImage ("game.dsk", blocks, problem));
        Assert::AreEqual (std::string ("A2retroNET reads only .po, .hdv and .2mg images."), problem);

        Assert::IsFalse (A2retroNetConfig::CheckImage ("disk.2mg", MakeTwoImg (280, 0, 64), problem), L"DOS order");
        Assert::IsFalse (A2retroNetConfig::CheckImage ("disk.2mg", MakeTwoImg (280, 1, 128), problem), L"data after a longer header");
        Assert::IsFalse (A2retroNetConfig::CheckImage ("big.hdv",  huge, problem));
        Assert::AreEqual (std::string ("A2retroNET reads at most 65535 blocks, and this image has 65536."), problem);
    }


    TEST_METHOD (MakePath_IsFromTheRootWithSlashes)
    {
        Assert::AreEqual (std::string ("games/infocom/zork.po"), A2retroNetConfig::MakePath (L"E:\\", L"E:\\games\\infocom\\zork.po"));
        Assert::AreEqual (std::string ("system.hdv"),            A2retroNetConfig::MakePath (L"E:\\", L"E:\\system.hdv"));
    }
};
