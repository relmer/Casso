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
//  "mame" runs MAME's apple2gs, installed with its ROMs.
//  MAME fails the read-back check of a whole-disk write, which DOS 3.3's
//  INIT and KEGS pass, so the write is skipped there. "kegs" runs KEGS in the
//  WSL instance Ubuntu-CassoEmu through /root/kegsrun/run.sh, with the modem
//  port connected to the Windows host at kPort. That KEGS build is patched:
//  $00 bytes from a socket are passed to the 8530, where stock KEGS drops
//  them, and slot 6 boots when slot 7 has no disk. The link runs at 9,600 baud.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkIIgsTests)
{
public:

    static constexpr uint16_t      kPort        = 19802;
    static constexpr DWORD         kBaud        = 9600;
    static constexpr DWORD         kConnectMs   = 180'000;
    static constexpr DWORD         kWholeDiskMs = 600'000;
    static constexpr DWORD         kFileMs      = 180'000;
    static constexpr size_t        kDiskBytes   = 143'360;
    static constexpr const char *  kD2Image     = "C:\\gs\\d2.dsk";
    static constexpr const char *  kD2Written   = "C:\\gs\\d2-written.dsk";
    static constexpr const char *  kD2Again     = "C:\\gs\\d2-again.dsk";
    static constexpr const wchar_t * kKegsStart = L"wsl.exe -d Ubuntu-CassoEmu -u root -- /root/kegsrun/run.sh";
    static constexpr const wchar_t * kKegsStop  = L"wsl.exe -d Ubuntu-CassoEmu -u root -- pkill -x xkegs";




    TEST_METHOD (AnAppleIIgs_RunsStory14sOperations)
    {
        std::string                        run      = EmulatorLink::GetSetting ("CASSO_IIGS_EMULATOR");
        std::filesystem::path              mame     = std::filesystem::path (EmulatorLink::GetSetting ("LOCALAPPDATA")) / "Programs" / "MAME" / "mame.exe";
        std::filesystem::path              folder   = std::filesystem::temp_directory_path() / "CassoIIgs";
        bool                               isKegs   = run == "kegs";
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



        if (run != "mame" && run != "kegs")
        {
            Logger::WriteMessage ("NOT RUN: set CASSO_IIGS_EMULATOR to mame or kegs to run an Apple IIgs\n");
            return;
        }

        //  The universal disk at the link's speed, drive 2's ProDOS disk with
        //  a file, and the image written to drive 2.
        options.baud = kBaud;
        options.dial = std::format ("ATDT127.0.0.1:{}", kPort);
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

        //  The connection listens; the emulator connects to it once it starts.
        settings.name      = "IIgs";
        settings.transport = LinkTransport::WifiModem;
        settings.address   = std::format ("127.0.0.1:{}", kPort);
        settings.baud      = kBaud;

        session = std::make_unique<CassoLinkSession> (factory, linkFolder, fileIo, sink);
        session->SetConnectTimeout (kConnectMs);
        AssertSucceeded (session->Start (settings));

        if (isKegs)
        {
            emulator.stopCommand = kKegsStop;
            emulator.Start (kKegsStart, folder);
        }
        else
        {
            Assert::IsTrue (std::filesystem::exists (mame), L"MAME is installed");
            emulator.Start (std::format (L"\"{}\" apple2gs -flop1 \"{}\" -flop2 \"{}\" -modem null_modem -bitb socket.127.0.0.1:{} "
                                         L"-video none -sound none -skip_gameinfo -seconds_to_run 3600",
                                         mame.wstring(), (folder / "d1.dsk").wstring(), (folder / "d2.dsk").wstring(), kPort),
                            mame.parent_path());
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
        if (isKegs)
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
