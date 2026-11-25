#include "Pch.h"

#include "AppleDriveRig.h"
#include "CassoExplorer/Model/CassoLinkShimBuilder.h"
#include "CassoLink/CassoLinkProDosDisk.h"
#include "CassoLink/LinkAudioBootstrap.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Tape/WavCodec.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  LinkDiskActionsTests
//
//  ActionsLinkDisk over a fake disk file system and a FakeApple: the boot
//  disk, the ProDOS and SOS disks and the cassette bootstrap, saved as files
//  or written to a drive.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (LinkDiskActionsTests)
{
public:

    using Rig = AppleDriveRig;

    //  A ProDOS disk with small files in place of PRODOS and BASIC.SYSTEM.
    static std::vector<Byte>  MakeUsersProDos()
    {
        static constexpr Byte  kSystemType = 0xFF;

        std::vector<Byte>  users (NibbleFormat::kImageByteSize, 0);

        AssertSucceeded (ProDosSkeleton::Write (users, "USERS"));
        AssertSucceeded (ProDosFileWriter::WriteFile (users, "PRODOS", kSystemType, 0x0000, std::vector<Byte> (3000, 0x4C)));
        AssertSucceeded (ProDosFileWriter::WriteFile (users, "BASIC.SYSTEM", kSystemType, 0x2000, std::vector<Byte> (10240, 0x60)));

        return users;
    }



    //  Waits until the queued operations are done, then runs their
    //  completions. The disk has no file system, so there is no catalog to
    //  list afterward.
    static void  FinishWrite (Rig & rig)
    {
        auto  until = std::chrono::steady_clock::now() + std::chrono::seconds (20);

        while (!rig.links->GetPendingJobs().empty() && std::chrono::steady_clock::now() < until)
        {
            std::this_thread::yield();
        }

        Assert::IsTrue (rig.links->RunCompletions().empty());
    }




    TEST_METHOD (SaveLinkToDisk_WritesTheBootDiskToTheDrive)
    {
        Rig                rig;
        std::vector<Byte>  disk;
        LinkShimOptions    options;

        options.baud = CassoLinkSettings::kDefaultBaud;
        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, disk));

        rig.Open (2);
        AssertSucceeded (rig.actions->GetLinkDisks().SaveLinkToDisk ("Test", 2).hr);
        FinishWrite (rig);

        Assert::IsTrue (rig.Drive (2) == disk, L"the drive is the universal boot disk");
        Assert::IsTrue (rig.io.files.empty(), L"the disk is made in memory, with no file");
    }




    TEST_METHOD (SaveLinkDiskImage_WritesTheImageOnThePc)
    {
        Rig                rig;
        std::vector<Byte>  disk;
        CassoLinkSettings  serial;
        LinkShimOptions    options;

        serial.baud  = 9600;
        options.baud = 9600;

        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, disk));
        AssertSucceeded (rig.actions->GetLinkDisks().SaveLinkDiskImage (L"C:\\Disks\\link.dsk", serial).hr);

        Assert::IsTrue (rig.io.files["C:\\Disks\\link.dsk"] == disk);
    }




    TEST_METHOD (SaveLinkDiskImage_WriteFails_ReportsTheMessage)
    {
        Rig                    rig;
        CassoLinkSettings      serial;
        ActionsTypes::Outcome  outcome;

        rig.io.failNextWrite  = true;
        rig.io.nextWriteError = E_FAIL;

        outcome = rig.actions->GetLinkDisks().SaveLinkDiskImage (L"C:\\Disks\\link.dsk", serial);

        Assert::IsTrue   (outcome.hr == E_FAIL);
        Assert::AreEqual (std::wstring (L"The CassoLink disk could not be saved."), outcome.message);
        Assert::AreEqual (0, outcome.written);
    }



    //  The CassoLink ProDOS disk made from a user's ProDOS disk has its
    //  PRODOS and BASIC.SYSTEM, the installer and the STARTUP that runs it;
    //  a ProDOS disk with neither is turned down with the reason. The user's
    //  disk is made here, with small files in place of the two.
    TEST_METHOD (SaveLinkProDosDisk_WritesADiskWithTheUsersProDos)
    {
        Rig                rig;
        CassoLinkSettings  serial;
        std::vector<Byte>  users = MakeUsersProDos();
        std::vector<Byte>  blank (NibbleFormat::kImageByteSize, 0);
        std::vector<Byte>  file;
        Byte               fileType = 0;
        Word               auxType  = 0;

        rig.io.files["C:/Disks/Users.dsk"]  = users;
        rig.io.stamps["C:/Disks/Users.dsk"] = FileStamp { users.size(), 1 };

        AssertSucceeded (rig.actions->GetLinkDisks().SaveLinkProDosDisk (L"C:/Disks/Users.dsk", L"C:/Disks/Link.dsk", serial).hr);

        for (const char * name : { "PRODOS", "BASIC.SYSTEM", "CASSOLINK", "STARTUP" })
        {
            AssertSucceeded (ProDosReader::ExtractFile (rig.io.files["C:/Disks/Link.dsk"], name, file, fileType, auxType));
        }

        AssertSucceeded (ProDosSkeleton::Write (blank, "EMPTY"));
        rig.io.files["C:/Disks/Empty.dsk"]  = blank;
        rig.io.stamps["C:/Disks/Empty.dsk"] = FileStamp { blank.size(), 1 };

        Assert::IsFalse (rig.actions->GetLinkDisks().SaveLinkProDosDisk (L"C:/Disks/Empty.dsk", L"C:/Disks/Link2.dsk", serial).Succeeded(), L"A disk without PRODOS is turned down");
    }




    //  The disk written to the drive is the one made for the speed of the
    //  link it is written over, not the default speed.
    TEST_METHOD (SaveLinkProDosToDisk_UsesTheSpeedOfTheLink)
    {
        static constexpr DWORD  kSlowBaud = 9600;

        Rig                rig;
        CassoLinkSettings  slow;
        std::vector<Byte>  users = MakeUsersProDos();
        std::vector<Byte>  built;

        slow.name      = "Test";
        slow.transport = LinkTransport::Emulated;
        slow.address   = "127.0.0.1:6502";
        slow.baud      = kSlowBaud;

        rig.links->SetLinks ({ slow });
        rig.io.files["C:/Disks/Users.dsk"]  = users;
        rig.io.stamps["C:/Disks/Users.dsk"] = FileStamp { users.size(), 1 };

        AssertSucceeded (CassoLinkProDosDisk::Make (users, "C:/Disks/Users.dsk", kSlowBaud, built));

        rig.Open (2);
        AssertSucceeded (rig.actions->GetLinkDisks().SaveLinkProDosToDisk ("Test", 2, L"C:/Disks/Users.dsk").hr);
        FinishWrite (rig);

        Assert::IsTrue (rig.Drive (2) == built, L"the drive has the disk made for the link's speed");
    }




    TEST_METHOD (SaveLinkProDosToDisk_WithoutAUsersDisk_ReportsAndWritesNothing)
    {
        Rig                    rig;
        std::vector<Byte>      before;
        ActionsTypes::Outcome  outcome;

        rig.Open (2);
        before  = rig.Drive (2);
        outcome = rig.actions->GetLinkDisks().SaveLinkProDosToDisk ("Test", 2, L"C:/Disks/Missing.dsk");

        Assert::IsFalse (outcome.Succeeded());
        Assert::IsTrue  (rig.links->GetPendingJobs().empty(), L"nothing was queued");
        Assert::IsTrue  (rig.Drive (2) == before);
    }




    TEST_METHOD (SaveCassetteBootstrap_SerialLink_WritesTheShimWav)
    {
        Rig                    rig;
        CassoLinkSettings      serial;
        std::vector<Byte>      built;
        std::string            builtCommand;
        std::string            command;
        ActionsTypes::Outcome  outcome;

        serial.transport = LinkTransport::Serial;

        AssertSucceeded (LinkAudioBootstrap::BuildWav (CassoLinkShimBuilder::MakeShimOptions (CassoLinkShimBuilder::GetPcName(), serial), built, builtCommand));

        outcome = rig.actions->GetLinkDisks().SaveCassetteBootstrap (L"C:\\Tapes\\link.wav", serial, command);

        AssertSucceeded (outcome.hr);
        Assert::AreEqual (1, outcome.written);
        Assert::IsFalse  (built.empty());
        Assert::IsTrue   (rig.io.files["C:\\Tapes\\link.wav"] == built);
        Assert::AreEqual (builtCommand, command);
    }




    TEST_METHOD (SaveCassetteBootstrap_CassetteLink_WritesTheProgramWav)
    {
        Rig                    rig;
        CassoLinkSettings      cassette;
        TapeAudio              audio;
        std::vector<Byte>      built;
        std::string            builtCommand;
        std::string            command;
        ActionsTypes::Outcome  outcome;

        cassette.transport = LinkTransport::Cassette;

        AssertSucceeded (LinkAudioBootstrap::BuildCassetteLinkAudio (audio, builtCommand));
        WavCodec::Encode (audio, built);

        outcome = rig.actions->GetLinkDisks().SaveCassetteBootstrap (L"C:\\Tapes\\link.wav", cassette, command);

        AssertSucceeded (outcome.hr);
        Assert::AreEqual (1, outcome.written);
        Assert::IsFalse  (built.empty());
        Assert::IsTrue   (rig.io.files["C:\\Tapes\\link.wav"] == built);
        Assert::AreEqual (builtCommand, command);
    }




    TEST_METHOD (SaveCassetteBootstrap_WriteFails_ReportsTheMessage)
    {
        Rig                    rig;
        CassoLinkSettings      serial;
        std::string            command;
        ActionsTypes::Outcome  outcome;

        rig.io.failNextWrite  = true;
        rig.io.nextWriteError = E_FAIL;

        outcome = rig.actions->GetLinkDisks().SaveCassetteBootstrap (L"C:\\Tapes\\link.wav", serial, command);

        Assert::IsTrue   (outcome.hr == E_FAIL);
        Assert::AreEqual (std::wstring (L"The cassette bootstrap could not be saved."), outcome.message);
        Assert::AreEqual (0, outcome.written);
    }
};
