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
//  CassoLinkEthernetTests
//
//  Story 14's operations over an original Uthernet or a LANceGS (SC-059), in
//  an emulator in the WSL instance Ubuntu-CassoEmu: the universal disk boots
//  on an enhanced //e with the card in slot 3, takes an address by DHCP, and
//  UDP 6502 on the instance's address is forwarded to it. A ProDOS disk with
//  a file is in drive 2. The disk is browsed, a file is copied each way, a
//  file renamed and deleted and a folder made, and the disk read to an image
//  and an image written to it.
//
//  CASSO_NET_EMULATOR selects the card; with it unset, nothing is run.
//
//  - "lancegs" runs MAME's apple2ee with a LANceGS on a TAP device.
//  - "uthernet" runs AppleWin's Linux build with an Uthernet over libslirp.
//
//  Both run through scripts\EmuTargets\Wsl.ps1, which also builds them. The
//  connection prompts only in its first few seconds, so it is started once
//  the universal disk waits.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkEthernetTests)
{
public:

    static constexpr uint16_t        kPort        = 6502;
    static constexpr DWORD           kConnectMs   = 120'000;
    static constexpr DWORD           kBootMs      = 60'000;
    static constexpr DWORD           kWholeDiskMs = 600'000;
    static constexpr DWORD           kFileMs      = 180'000;
    static constexpr size_t          kDiskBytes   = 143'360;
    static constexpr const char *    kD2Image     = "C:\\net\\d2.dsk";
    static constexpr const char *    kD2Written   = "C:\\net\\d2-written.dsk";
    static constexpr const char *    kD2Again     = "C:\\net\\d2-again.dsk";
    static constexpr const wchar_t * kScript      = L"Wsl.ps1";




    TEST_METHOD (AnEthernetCard_RunsStory14sOperations)
    {
        std::string                        run      = EmulatorLink::GetSetting ("CASSO_NET_EMULATOR");
        std::filesystem::path              folder   = std::filesystem::temp_directory_path() / "CassoNet";
        std::string                        address;
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



        if (run != "lancegs" && run != "uthernet")
        {
            Logger::WriteMessage ("NOT RUN: set CASSO_NET_EMULATOR to lancegs or uthernet to run a network card\n");
            return;
        }

        address = EmulatorLink::ReadCommandLine (EmulatorLink::MakeTargetCommand (kScript, L"-Address").c_str());
        Assert::IsFalse (address.empty(), L"the WSL instance has an address");

        //  The universal disk, drive 2's ProDOS disk with a file, and the
        //  image written to drive 2.
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

        emulator.stopCommand = EmulatorLink::MakeTargetCommand (kScript, L"-Stop");
        emulator.Start (EmulatorLink::MakeTargetCommand (kScript, std::format (L"-Target {} -Disk1 \"{}\" -Disk2 \"{}\"",
                                                                               (run == "lancegs") ? L"LanceGs" : L"Uthernet",
                                                                               (folder / "d1.dsk").wstring(), (folder / "d2.dsk").wstring())),
                        folder);

        std::this_thread::sleep_for (std::chrono::milliseconds (kBootMs));

        settings.name      = "Ethernet";
        settings.transport = LinkTransport::Udp;
        settings.address   = std::format ("{}:{}", address, kPort);

        session = std::make_unique<CassoLinkSession> (factory, linkFolder, fileIo, sink);
        session->SetConnectTimeout (kConnectMs);
        AssertSucceeded (session->Start (settings));

        Assert::IsTrue (sink.WaitForState (LinkState::Ready, kConnectMs), L"the Apple connects");

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
        EmulatorLink::Finish (sink, session->WriteDisk (2, kD2Written), kWholeDiskMs);
        EmulatorLink::Finish (sink, session->ReadDisk (2, kD2Again, false), kWholeDiskMs);
        Assert::IsTrue (fileIo.files[kD2Again] == fileIo.files[kD2Written], L"drive 2 has the image written to it");

        session.reset();
    }
};
