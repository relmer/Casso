#include "Pch.h"

#include "EhmTestHelper.h"
#include "EmulatorLink.h"
#include "FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/LinkProgram.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Seams/Win32ByteStreamFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkIIgsTests
//
//  Story 14's operations on an Apple IIgs in another emulator (SC-059): the
//  universal disk boots headless, with the modem port on a TCP socket to a
//  Wi-Fi modem connection, which listens. A ProDOS disk with a file is in
//  drive 2. The disk is browsed, a file is copied each way, a file renamed
//  and deleted and a folder made, and the disk read to an image and an image
//  written to it.
//
//  CASSO_IIGS_EMULATOR selects the emulator; with it unset, nothing is run.
//
//  - "mame" runs MAME's apple2gs, installed with its ROMs, through
//    scripts\EmuTargets\Mame.ps1. MAME fails the read-back check of a
//    whole-disk write, which DOS 3.3's INIT, KEGS and GSplus pass, so the
//    write is skipped on MAME.
//  - "mame-ssc" runs it with a Super Serial Card in slot 7. The card's
//    socket listens, and a TCP serial connection is started once the
//    universal disk waits.
//  - "kegs" and "gsplus" run KEGS and GSplus in the WSL instance
//    Ubuntu-CassoEmu through scripts\EmuTargets\Wsl.ps1, which also builds
//    them, with the modem port connected to the Windows host at kPort.
//
//  The link starts at 9,600 baud.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkIIgsTests)
{
public:

    static constexpr uint16_t        kPort        = 19802;
    static constexpr DWORD           kBaud        = 9600;
    static constexpr DWORD           kConnectMs   = 180'000;
    static constexpr DWORD           kBootMs      = 60'000;
    static constexpr DWORD           kWholeDiskMs = 600'000;
    static constexpr DWORD           kFileMs      = 180'000;
    static constexpr size_t          kDiskBytes   = 143'360;
    static constexpr const char *    kD2Image     = "C:\\gs\\d2.dsk";
    static constexpr const char *    kD2Written   = "C:\\gs\\d2-written.dsk";
    static constexpr const char *    kD2Again     = "C:\\gs\\d2-again.dsk";




    TEST_METHOD (AnAppleIIgs_RunsStory14sOperations)
    {
        std::string                        run      = EmulatorLink::GetSetting ("CASSO_IIGS_EMULATOR");
        std::filesystem::path              folder   = std::filesystem::temp_directory_path() / "CassoIIgs";
        bool                               isSsc    = run == "mame-ssc";
        bool                               isMame   = run == "mame" || isSsc;
        const wchar_t                    * script   = isMame ? L"Mame.ps1" : L"Wsl.ps1";
        const wchar_t                    * target   = isSsc ? L"IIgsSsc" : isMame ? L"IIgsModem" : (run == "kegs") ? L"Kegs" : L"Gsplus";
        LinkShimOptions                    options;
        std::vector<Byte>                  linkDisk;
        std::vector<Byte>                  blank (kDiskBytes, 0);
        std::vector<Byte>                  d2 (kDiskBytes, 0);
        std::vector<Byte>                  toWrite (kDiskBytes, 0);
        std::vector<Byte>                  next;
        FilePayload                        copied   = EmulatorLink::MakeBinary (3000, 5);
        FilePayload                        onDisk   = EmulatorLink::MakeBinary (1500, 9);
        Win32ByteStreamFactory             factory;
        InMemoryCassoLinkFolder            linkFolder;
        FakeDiskFileIo                     fileIo;
        EmulatorLink::Sink                 sink;
        std::unique_ptr<CassoLinkSession>  session;
        CassoLinkSettings                  settings;
        EmulatorLink::Process              emulator;



        if (!isMame && run != "kegs" && run != "gsplus")
        {
            Logger::WriteMessage ("NOT RUN: set CASSO_IIGS_EMULATOR to mame, mame-ssc, kegs or gsplus to run an Apple IIgs\n");
            return;
        }

        //  The universal disk at the link's speed, drive 2's ProDOS disk with
        //  a file, and the image written to drive 2.
        options.baud = kBaud;
        options.dial = isSsc ? std::string() : std::format ("ATDT127.0.0.1:{}", kPort);
        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, linkDisk));
        AssertSucceeded (ProDosSkeleton::Write (blank, "DRIVETWO"));
        AssertSucceeded (ProDosVolume (blank).Write (FilePath::Parse ("HELLO"), onDisk, d2));
        AssertSucceeded (ProDosSkeleton::Write (toWrite, "WRITTEN"));
        AssertSucceeded (ProDosVolume (toWrite).Write (FilePath::Parse ("ONDISK"), onDisk, next));
        toWrite = std::move (next);

        std::filesystem::create_directories (folder);
        EmulatorLink::WriteHostFile (folder / "d1.dsk", linkDisk);
        EmulatorLink::WriteHostFile (folder / "d2.dsk", d2);

        fileIo.files[kD2Written]  = toWrite;
        fileIo.stamps[kD2Written] = FileStamp { toWrite.size(), 1 };

        //  A modem port's connection listens, and the emulator connects to it
        //  once it starts. A Super Serial Card has no modem to answer a dial,
        //  so MAME listens on the card's socket and the connection is a TCP
        //  serial link to it, which prompts first.
        settings.name      = "IIgs";
        settings.transport = isSsc ? LinkTransport::Tcp : LinkTransport::WifiModem;
        settings.address   = std::format ("127.0.0.1:{}", kPort);
        settings.baud      = kBaud;

        session = std::make_unique<CassoLinkSession> (factory, linkFolder, fileIo, sink);
        session->SetConnectTimeout (kConnectMs);

        if (!isSsc)
        {
            AssertSucceeded (session->Start (settings));
        }

        emulator.stopCommand = EmulatorLink::MakeTargetCommand (script, std::format (L"-Target {} -Stop", target));
        emulator.Start (EmulatorLink::MakeTargetCommand (script, std::format (L"-Target {} -Disk1 \"{}\" -Disk2 \"{}\" -Port {}{}", target,
                                                                              (folder / "d1.dsk").wstring(), (folder / "d2.dsk").wstring(), kPort,
                                                                              isMame ? L" -Headless" : L"")),
                        folder);

        //  The connection prompts only in its first few seconds, so it is
        //  started once the universal disk waits, as a user boots the disk and
        //  then connects.
        if (isSsc)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (kBootMs));
            AssertSucceeded (session->Start (settings));
        }

        Assert::IsTrue (sink.WaitForState (LinkState::Ready, kConnectMs), L"the IIgs connects");

        //  The disk read to an image, as it is in the emulator.
        EmulatorLink::Finish (sink, session->ReadDisk (2, kD2Image, false), kWholeDiskMs);
        Assert::IsTrue (fileIo.files[kD2Image] == d2, L"drive 2's image is its disk");

        //  Browse, and a file copied from the Apple.
        auto  catalog = EmulatorLink::Finish (sink, session->Enumerate (2, FilePath()), kFileMs);

        Assert::IsTrue (EmulatorLink::GetNames (catalog->listing) == std::vector<std::string> { "HELLO" }, L"drive 2 has HELLO");

        auto  read = EmulatorLink::Finish (sink, session->ReadFile (2, FilePath::Parse ("HELLO")), kFileMs);

        Assert::IsTrue (read->file.bytes == onDisk.bytes, L"HELLO copied from the Apple is the disk's");

        //  A file copied to the Apple, read back, renamed and deleted, and a
        //  folder made.
        EmulatorLink::Finish (sink, session->WriteFile (2, FilePath::Parse ("COPIED"), copied), kFileMs);

        auto  back = EmulatorLink::Finish (sink, session->ReadFile (2, FilePath::Parse ("COPIED")), kFileMs);

        Assert::IsTrue (back->file.bytes == copied.bytes, L"the file copied to the Apple is read back");

        EmulatorLink::Finish (sink, session->Rename (2, FilePath::Parse ("COPIED"), "RENAMED"), kFileMs);
        EmulatorLink::Finish (sink, session->MakeDirectory (2, FilePath::Parse ("FOLDER")), kFileMs);
        EmulatorLink::Finish (sink, session->Delete (2, FilePath::Parse ("RENAMED")), kFileMs);

        catalog = EmulatorLink::Finish (sink, session->Enumerate (2, FilePath()), kFileMs);
        Assert::IsTrue (EmulatorLink::GetNames (catalog->listing) == std::vector<std::string> { "HELLO", "FOLDER" }, L"drive 2 has HELLO and the folder");

        //  An image written to drive 2 and read back.
        if (!isMame)
        {
            EmulatorLink::Finish (sink, session->WriteDisk (2, kD2Written), kWholeDiskMs);
            EmulatorLink::Finish (sink, session->ReadDisk (2, kD2Again, false), kWholeDiskMs);
            Assert::IsTrue (fileIo.files[kD2Again] == fileIo.files[kD2Written], L"drive 2 has the image written to it");
        }
        else
        {
            Logger::WriteMessage ("NOT RUN on MAME: writing a whole disk, whose read-back check fails in MAME's apple2gs\n");
        }

        session.reset();
    }
};
