#include "Pch.h"

#include "CassoExplorer/Model/DiskOperations.h"
#include "CommandLineOptions.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/DiskHelpPage.h"
#include "../EmuTests/FakeDiskFileIo.h"
#include "RepairFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using Command = CommandLineOptions::DiskOptions::Command;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskRepairCommandTests
//
//  `disk repair` and the verdict `disk test` adds: the verdict, the
//  findings and the changes reported, the copy written beside the image
//  or to the path given, nothing written with --list, sectors that no file uses
//  marked used with --no-free-unowned, and a status of 1 for a volume that
//  can't be repaired.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskRepairCommandTests)
{
public:

    static constexpr const char *  kImage = "C:\\Disks\\Game.dsk";
    static constexpr const char *  kCopy  = "C:\\Disks\\Game (repaired).dsk";



    static DiskCommandResult Run (FakeDiskFileIo & io, Command command, const std::string & copy = "", bool isListOnly = false, bool isKeeping = false)
    {
        CommandLineOptions  options = DiskOperations::MakeOptions (command, kImage);
        DiskCommandRunner   runner (io);



        options.disk.hostFile      = copy;
        options.disk.listOnly      = isListOnly;
        options.disk.noFreeUnowned = isKeeping;

        return runner.Run (options);
    }



    static bool Contains (const std::string & text, const char * part)
    {
        return text.find (part) != std::string::npos;
    }



    TEST_METHOD (RepairReportsTheVerdictAndTheChangesAndWritesTheCopy)
    {
        FakeDiskFileIo     io;
        RepairFixture      fixture = RepairFixtures::MakeDos33Bitmap();
        DiskCommandResult  result;



        io.files[kImage] = fixture.damaged;
        result           = Run (io, Command::Repair);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, Widen (result.diagnostics).c_str());
        Assert::IsTrue   (Contains (result.output, "Game.dsk: file system: Bad\n"), Widen (result.output).c_str());
        Assert::IsTrue   (Contains (result.output, "but the bitmap marks it free"));
        Assert::IsTrue   (Contains (result.output, "repair: Mark 1 sector HELLO uses as used in the bitmap\n"));
        Assert::IsTrue   (Contains (result.output, "repaired copy written to C:\\Disks\\Game (repaired).dsk\n"));
        Assert::IsTrue   (io.files[kCopy] == fixture.before, L"the copy is the image before the damage");
        Assert::IsTrue   (io.files[kImage] == fixture.damaged, L"the image is as it was");
    }



    TEST_METHOD (RepairWritesToTheCopyGiven)
    {
        FakeDiskFileIo     io;
        RepairFixture      fixture = RepairFixtures::MakeDos33CountDiffers();
        DiskCommandResult  result;



        io.files[kImage] = fixture.damaged;
        result           = Run (io, Command::Repair, "C:\\Out\\fixed.dsk");

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue   (io.files["C:\\Out\\fixed.dsk"] == fixture.before);
        Assert::IsFalse  (io.Exists (kCopy));
    }



    TEST_METHOD (ListReportsTheChangesAndWritesNothing)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;



        io.files[kImage] = RepairFixtures::MakeDos33Bitmap().damaged;
        result           = Run (io, Command::Repair, "", true);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue   (Contains (result.output, "repair: Mark 1 sector that no file uses as free in the bitmap\n"));
        Assert::AreEqual (static_cast<size_t> (1), io.files.size(), L"no copy is written");
    }



    TEST_METHOD (NoFreeUnownedMarksSectorsNoFileUsesAsUsed)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;



        io.files[kImage] = RepairFixtures::MakeDos33Bitmap().damaged;
        result           = Run (io, Command::Repair, "", true, true);

        Assert::IsFalse (Contains (result.output, "no file uses as free"));
        Assert::IsTrue  (Contains (result.output, "repair: Mark 1 sector HELLO uses as used in the bitmap\n"));
    }



    TEST_METHOD (AVolumeThatCantBeRepairedExitsWithComplaints)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;



        io.files[kImage] = RepairFixtures::MakeDos33ChainOffDisk().damaged;
        result           = Run (io, Command::Repair);

        Assert::AreEqual (DiskCommandResult::kWithComplaints, result.exitStatus);
        Assert::IsTrue   (Contains (result.output, "not repaired: A file's data can't be recovered by changing the bookkeeping."), Widen (result.output).c_str());
        Assert::AreEqual (static_cast<size_t> (1), io.files.size(), L"no copy is written");
    }



    TEST_METHOD (TestAddsTheVerdictAndItsFinding)
    {
        FakeDiskFileIo     io;
        DiskCommandResult  result;



        io.files[kImage] = RepairFixtures::MakeDos33();
        result           = Run (io, Command::Test);
        Assert::IsTrue   (Contains (result.output, "Game.dsk: file system: OK\n"), Widen (result.output).c_str());
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        io.files[kImage] = RepairFixtures::MakeDos33CountDiffers().damaged;
        result           = Run (io, Command::Test);
        Assert::IsTrue   (Contains (result.output, "Game.dsk: file system: Bad, HELLO: its catalog count is 9 sectors, and the file uses 4\n"), Widen (result.output).c_str());
        Assert::AreEqual (DiskCommandResult::kWithComplaints, result.exitStatus);
    }



    TEST_METHOD (TheHelpPageHasRepairAndItsOptions)
    {
        std::string  help = DiskHelpPage::BuildHelpText();



        Assert::IsTrue (Contains (help, "repair"));
        Assert::IsTrue (Contains (help, "--no-free-unowned"));
        Assert::IsTrue (Contains (help, "--list"));
    }



    static std::wstring Widen (const std::string & text)
    {
        return TextEncoding::NarrowToWide (text);
    }
};
