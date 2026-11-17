#include "Pch.h"

#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "UiTests/InMemoryFileSystem.h"
#include "CassoExplorer/CassoExplorerActions.h"
#include "CassoExplorer/CassoExplorerBrowser.h"
#include "Devices/Disk/A2retroNetConfig.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  A2PicoCardTests
//
//  An A2Pico card made and filled through Casso Explorer's actions, on an
//  in-memory card at E:, with Apple's ProDOS Users Disk: the starter
//  A2retroNET.txt, the disk copied in DOS order as it is distributed, Write
//  as .po, the .po set as drive 1, and the drive's image checked as the
//  firmware opens it. Casso has no SmartPort card, so the .po is booted from
//  a Disk II on a //e, which reads the same blocks in ProDOS order. The owner
//  has no A2Pico, so nothing here runs on one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (A2PicoCardTests)
{
public:

    struct Card
    {
        InMemoryFileSystem    fs;
        FakeDiskFileIo        io;
        CassoExplorerBrowser  browser { fs, io };
        CassoExplorerActions  actions { browser, fs };

        void Seed (const std::vector<Byte> & bytes, const std::wstring & path)
        {
            std::string  narrow (path.begin(), path.end());



            AssertSucceeded (fs.WriteAllText (path, std::string (bytes.begin(), bytes.end())));
            io.files[narrow]  = bytes;
            io.stamps[narrow] = FileStamp { bytes.size(), 100 };
        }

        //  A file the actions wrote through the disk I/O, put where the
        //  card's folder listing finds it too.
        void List (const std::wstring & path)
        {
            std::string  narrow (path.begin(), path.end());



            AssertSucceeded (fs.WriteAllText (path, std::string (io.files[narrow].begin(), io.files[narrow].end())));
            io.stamps[narrow] = FileStamp { io.files[narrow].size(), 100 };
        }
    };


    //  Runs the machine a slice at a time until a row of the text screen
    //  contains `needle`, or until `cycleCap` cycles have run.
    static bool RunUntilScreenShows (MachineHost & host, const std::string & needle, uint64_t cycleCap)
    {
        constexpr uint64_t  kSlice = 1'000'000ULL;

        std::vector<std::string>  rows;
        uint64_t                  spent    = 0;
        bool                      onScreen = false;



        while (!onScreen && spent < cycleCap)
        {
            spent += host.RunCycles (kSlice);

            rows.clear();
            GuestSession::CollectRows (host, rows);
            onScreen = GuestSession::AnyRowContains (rows, needle);
        }

        return onScreen;
    }


    TEST_METHOD (UsersDisk_IsWrittenAsAPoSetAsADriveAndBoots)
    {
        Card                           card;
        TestMachine                    machine ("Apple2eEnhanced");
        std::vector<Byte>              usersDisk = GuestSession::RequireProDosUsersDisk();
        std::vector<std::wstring>      convertible;
        std::wstring                   written;
        CassoExplorerActions::Outcome  outcome;
        ActionsA2retroNet::Card        read;
        A2retroNetConfig               config;
        std::vector<Byte>              po;



        //  The starter file Initialize card writes for an A2Pico.
        outcome = card.actions.CreateA2retroNetConfig (L"E:\\");
        Assert::IsTrue (outcome.Succeeded(), outcome.message.c_str());
        card.List (L"E:\\A2retroNET.txt");

        //  The disk as it is distributed, a ProDOS volume in DOS order, which
        //  A2retroNET doesn't read.
        card.Seed (usersDisk, L"E:\\apple\\users.dsk");

        AssertSucceeded  (card.actions.GetA2retroNet().FindConvertible (L"E:\\apple", convertible));
        Assert::AreEqual ((size_t) 1, convertible.size());

        outcome = card.actions.GetA2retroNet().ConvertToPo (convertible[0], written);
        Assert::IsTrue   (outcome.Succeeded(), outcome.message.c_str());
        Assert::AreEqual (std::wstring (L"E:\\apple\\users.po"), written);
        card.List (written);

        //  Set as drive 1, as A2retroNET drives sets it.
        AssertSucceeded (card.actions.ReadA2PicoCard (L"E:\\", read));
        config          = read.config;
        config.paths[0] = A2retroNetConfig::MakePath (L"E:\\", written);
        outcome         = card.actions.SaveA2retroNetConfig (L"E:\\", config);
        Assert::IsTrue (outcome.Succeeded(), outcome.message.c_str());
        card.List (L"E:\\A2retroNET.txt");

        AssertSucceeded  (card.actions.ReadA2PicoCard (L"E:\\", read));
        Assert::AreEqual (std::string ("apple/users.po"), read.drives[0].path);
        Assert::IsTrue   (read.drives[0].isPresent);
        Assert::IsTrue   (read.drives[0].problem.empty(), read.drives[0].problem.c_str());
        Assert::AreEqual (std::wstring (L"A2Pico card. 1 of its 8 A2retroNET drives have an image."), ActionsA2retroNet::Summarize (read));

        //  The .po boots ProDOS from a Disk II, read in ProDOS order.
        po = card.io.files["E:\\apple\\users.po"];

        AssertSucceeded (machine.GetDiskStore().MountFromBytes (6, 0, "users.po", DiskFormat::Po, po));
        machine.PowerCycle();
        GuestSession::StartFromPowerOn (machine);

        Assert::IsTrue (RunUntilScreenShows (machine, "PRODOS", 40'000'000ULL), L"the .po boots ProDOS");
    }
};
