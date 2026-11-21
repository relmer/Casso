#include "Pch.h"

#include "EhmTestHelper.h"
#include "EmulatorLink.h"
#include "FakeDiskFileIo.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/LinkProgram.h"
#include "CassoLink/Session/CassoLinkBlockDevices.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Seams/Win32ByteStreamFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkBlockEmulatorTests
//
//  Block devices over the link in another emulator (SC-054): each volume the
//  emulator mounted is listed, read through the link to an image identical
//  to the one mounted, and a file put on the 800K volume through the link is
//  listed and read back by the Apple.
//
//  CASSO_BLOCK_EMULATOR selects the target; with it unset, nothing is run.
//
//  - "iicplus" runs MAME's //c Plus through scripts\EmuTargets\Mame.ps1,
//    with an 800K volume in the internal 3.5-inch drive and the universal
//    disk in the external 5.25-inch drive. The modem port's socket listens,
//    and a TCP serial connection to it prompts until the universal disk
//    sends its Ready.
//  - "harddisk" runs AppleWin's Linux build with a 32 MB and an 800K volume
//    on a hard disk card, through scripts\EmuTargets\Wsl.ps1. The link runs
//    over its Uthernet, with UDP 6502 on the WSL instance's address forwarded
//    to the Apple. The Apple boots slot 7 first, so the 32 MB volume's boot
//    block jumps to the Disk II in slot 6. With CASSO_VISIBLE set, AppleWin's
//    window is shown.
//
//  With CASSO_BLOCK_RANDOM set, the volumes' free blocks are random bytes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkBlockEmulatorTests)
{
public:

    static constexpr uint16_t        kPort         = 19803;
    static constexpr uint16_t        kUdpPort      = 6502;
    static constexpr DWORD           kBaud         = 9600;
    static constexpr DWORD           kConnectMs    = 180'000;
    static constexpr DWORD           kVolumeMs     = 3'600'000;
    static constexpr DWORD           kFileMs       = 180'000;
    static constexpr size_t          kBlockBytes   = 512;
    static constexpr uint32_t        k800KBlocks   = 1600;
    static constexpr uint32_t        k32MBlocks    = ProDosSkeleton::kMaxBlocks;
    static constexpr uint32_t        kRandomFrom   = 64;

    //  A boot block that boots the Disk II in slot 6: the boot ROM loads it
    //  at $0800 and runs it from $0801, JMP $C600.
    static constexpr Byte            kBootSlot6[]  = { 0x01, 0x4C, 0x00, 0xC6 };




    //  A blank ProDOS volume of `blocks` with a file on it. With
    //  CASSO_BLOCK_RANDOM set, its blocks from kRandomFrom on, all free, are
    //  filled with pseudo-random bytes, so a transfer that compresses runs of
    //  a byte gains nothing from the empty space.
    static std::vector<Byte>  MakeVolume (uint32_t blocks, const char * name, const FilePayload & file)
    {
        std::vector<Byte>  volume ((size_t) blocks * kBlockBytes, 0);
        std::vector<Byte>  next;
        uint32_t           seed   = 0x2545F491;



        AssertSucceeded (ProDosSkeleton::Write (volume, name));
        AssertSucceeded (ProDosVolume (volume).Write (FilePath::Parse ("HELLO"), file, next));

        if (!EmulatorLink::GetSetting ("CASSO_BLOCK_RANDOM").empty())
        {
            for (size_t at = (size_t) kRandomFrom * kBlockBytes; at < next.size(); at++)
            {
                seed     = seed * 1664525 + 1013904223;
                next[at] = (Byte) (seed >> 24);
            }
        }

        return next;
    }


    //  The session's drive for the listed device with `blocks`; -1 with none.
    static int  FindDrive (const CassoLinkSession & session, uint32_t blocks)
    {
        std::vector<CassoLinkBlockDevice>  devices = session.GetBlockDevices();
        int                                drive   = -1;



        for (size_t i = 0; i < devices.size() && drive < 0; i++)
        {
            if (devices[i].blocks == blocks)
            {
                drive = CassoLinkBlockDevices::kFirstDrive + (int) i;
            }
        }

        return drive;
    }


    //  The device at drive read to path through the link, with the time
    //  it took and its blocks a second written to the test's output.
    static void  ReadVolume (EmulatorLink::Sink & sink, CassoLinkSession & session, int drive, const char * path, uint32_t blocks)
    {
        auto         started = std::chrono::steady_clock::now();
        double       seconds = 0;



        EmulatorLink::Finish (sink, session.ReadBlockDevice (drive, path, false), kVolumeMs);

        seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - started).count();
        std::string  line = std::format ("{}: {} blocks in {:.1f} s, {:.1f} blocks a second\n", path, blocks, seconds, blocks / seconds);

        Logger::WriteMessage (line.c_str());
        std::ofstream (std::filesystem::temp_directory_path() / "CassoBlock" / "timing.txt", std::ios::app) << line;
    }




    TEST_METHOD (TheEmulatorsBlockDevices_ReadAsMounted_AndTakeAFile)
    {
        std::string                        run       = EmulatorLink::GetSetting ("CASSO_BLOCK_EMULATOR");
        std::filesystem::path              folder    = std::filesystem::temp_directory_path() / "CassoBlock";
        bool                               isPlus    = run == "iicplus";
        LinkShimOptions                    options;
        std::vector<Byte>                  linkDisk;
        FilePayload                        onDisk    = EmulatorLink::MakeBinary (1500, 9);
        FilePayload                        copied    = EmulatorLink::MakeBinary (3000, 5);
        std::vector<Byte>                  smallVolume = MakeVolume (k800KBlocks, "SMALL", onDisk);
        std::vector<Byte>                  largeVolume;
        Win32ByteStreamFactory             factory;
        InMemoryCassoLinkFolder            linkFolder;
        FakeDiskFileIo                     fileIo;
        EmulatorLink::Sink                 sink;
        std::unique_ptr<CassoLinkSession>  session;
        CassoLinkSettings                  settings;
        EmulatorLink::Process              emulator;
        std::wstring                       start;
        int                                smallDrive = -1;
        int                                largeDrive = -1;



        if (!isPlus && run != "harddisk")
        {
            Logger::WriteMessage ("NOT RUN: set CASSO_BLOCK_EMULATOR to iicplus or harddisk to run an emulator's block devices\n");
            return;
        }

        options.baud = kBaud;
        AssertSucceeded (LinkProgram::BuildUniversalBootDisk (options, linkDisk));

        std::filesystem::create_directories (folder);
        EmulatorLink::WriteHostFile (folder / "d1.dsk", linkDisk);
        EmulatorLink::WriteHostFile (folder / "small.po", smallVolume);

        settings.name      = "Blocks";
        settings.baud      = kBaud;

        if (isPlus)
        {
            settings.transport   = LinkTransport::Tcp;
            settings.address     = std::format ("127.0.0.1:{}", kPort);
            start                = EmulatorLink::MakeTargetCommand (L"Mame.ps1", std::format (L"-Target IIcPlus -Disk1 \"{}\" -Disk2 \"{}\" -Port {} -Headless",
                                                                                              (folder / "d1.dsk").wstring(), (folder / "small.po").wstring(), kPort));
            emulator.stopCommand = EmulatorLink::MakeTargetCommand (L"Mame.ps1", L"-Target IIcPlus -Stop");
        }
        else
        {
            largeVolume = MakeVolume (k32MBlocks, "LARGE", onDisk);
            std::copy (std::begin (kBootSlot6), std::end (kBootSlot6), largeVolume.begin());
            EmulatorLink::WriteHostFile (folder / "large.hdv", largeVolume);

            settings.transport   = LinkTransport::Udp;
            settings.address     = std::format ("{}:{}", EmulatorLink::ReadCommandLine (EmulatorLink::MakeTargetCommand (L"Wsl.ps1", L"-Address").c_str()), kUdpPort);
            start                = EmulatorLink::MakeTargetCommand (L"Wsl.ps1", std::format (L"-Target HardDisk -Disk1 \"{}\" -Disk2 \"{}\" -Disk3 \"{}\"{}",
                                                                                             (folder / "d1.dsk").wstring(), (folder / "large.hdv").wstring(), (folder / "small.po").wstring(),
                                                                                             EmulatorLink::GetSetting ("CASSO_VISIBLE").empty() ? L"" : L" -Visible"));
            emulator.stopCommand = EmulatorLink::MakeTargetCommand (L"Wsl.ps1", L"-Stop");
        }

        emulator.Start (start, folder);

        session = std::make_unique<CassoLinkSession> (factory, linkFolder, fileIo, sink);
        session->SetConnectTimeout (kConnectMs);
        AssertSucceeded (session->Start (settings));

        Assert::IsTrue (sink.WaitForState (LinkState::Ready, kConnectMs), L"the Apple connects");

        EmulatorLink::Finish (sink, session->ListBlockDevices(), kFileMs);

        smallDrive = FindDrive (*session, k800KBlocks);
        Assert::IsTrue (smallDrive >= 0, L"the 800K volume is listed");

        //  Each volume read through the link is the image mounted.
        ReadVolume (sink, *session, smallDrive, "C:\\blocks\\small.po", k800KBlocks);
        Assert::IsTrue (fileIo.files["C:\\blocks\\small.po"] == smallVolume, L"the 800K volume as mounted");

        if (!isPlus)
        {
            largeDrive = FindDrive (*session, k32MBlocks);
            Assert::IsTrue (largeDrive >= 0, L"the 32 MB volume is listed");

            ReadVolume (sink, *session, largeDrive, "C:\\blocks\\large.hdv", k32MBlocks);
            Assert::IsTrue (fileIo.files["C:\\blocks\\large.hdv"] == largeVolume, L"the 32 MB volume as mounted");
        }

        //  A file put through the link is listed and read back by the Apple.
        EmulatorLink::Finish (sink, session->WriteFile (smallDrive, FilePath::Parse ("COPIED"), copied), kFileMs);

        auto  catalog = EmulatorLink::Finish (sink, session->Enumerate (smallDrive, FilePath()), kFileMs);

        Assert::IsTrue (EmulatorLink::GetNames (catalog->listing) == std::vector<std::string> { "HELLO", "COPIED" }, L"the volume lists the file put on it");

        auto  back = EmulatorLink::Finish (sink, session->ReadFile (smallDrive, FilePath::Parse ("COPIED")), kFileMs);

        Assert::IsTrue (back->file.bytes == copied.bytes, L"the file put on the volume is read back");

        session.reset();
    }
};
