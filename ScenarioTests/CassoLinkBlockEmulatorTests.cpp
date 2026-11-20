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
//  - "harddisk" runs AppleWin with a 32 MB and an 800K volume on a hard disk
//    card, through scripts\EmuTargets\Wsl.ps1, its Super Serial Card on TCP
//    port 1977 of the WSL instance.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkBlockEmulatorTests)
{
public:

    static constexpr uint16_t        kPort         = 19803;
    static constexpr uint16_t        kAppleWinPort = 1977;
    static constexpr DWORD           kBaud         = 9600;
    static constexpr DWORD           kConnectMs    = 180'000;
    static constexpr DWORD           kVolumeMs     = 3'600'000;
    static constexpr DWORD           kFileMs       = 180'000;
    static constexpr size_t          kBlockBytes   = 512;
    static constexpr uint32_t        k800KBlocks   = 1600;
    static constexpr uint32_t        k32MBlocks    = ProDosSkeleton::kMaxBlocks;




    //  A blank ProDOS volume of `blocks` with a file on it.
    static std::vector<Byte>  MakeVolume (uint32_t blocks, const char * name, const FilePayload & file)
    {
        std::vector<Byte>  volume ((size_t) blocks * kBlockBytes, 0);
        std::vector<Byte>  next;



        AssertSucceeded (ProDosSkeleton::Write (volume, name));
        AssertSucceeded (ProDosVolume (volume).Write (FilePath::Parse ("HELLO"), file, next));
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
        settings.transport = LinkTransport::Tcp;
        settings.baud      = kBaud;

        if (isPlus)
        {
            settings.address     = std::format ("127.0.0.1:{}", kPort);
            start                = EmulatorLink::MakeTargetCommand (L"Mame.ps1", std::format (L"-Target IIcPlus -Disk1 \"{}\" -Disk2 \"{}\" -Port {} -Headless",
                                                                                              (folder / "d1.dsk").wstring(), (folder / "small.po").wstring(), kPort));
            emulator.stopCommand = EmulatorLink::MakeTargetCommand (L"Mame.ps1", L"-Target IIcPlus -Stop");
        }
        else
        {
            largeVolume = MakeVolume (k32MBlocks, "LARGE", onDisk);
            EmulatorLink::WriteHostFile (folder / "large.hdv", largeVolume);

            settings.address     = std::format ("{}:{}", EmulatorLink::ReadCommandLine (EmulatorLink::MakeTargetCommand (L"Wsl.ps1", L"-Address").c_str()), kAppleWinPort);
            start                = EmulatorLink::MakeTargetCommand (L"Wsl.ps1", std::format (L"-Target HardDisk -Disk1 \"{}\" -Disk2 \"{}\" -Disk3 \"{}\"",
                                                                                             (folder / "d1.dsk").wstring(), (folder / "large.hdv").wstring(), (folder / "small.po").wstring()));
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
        EmulatorLink::Finish (sink, session->ReadBlockDevice (smallDrive, "C:\\blocks\\small.po", false), kVolumeMs);
        Assert::IsTrue (fileIo.files["C:\\blocks\\small.po"] == smallVolume, L"the 800K volume as mounted");

        if (!isPlus)
        {
            largeDrive = FindDrive (*session, k32MBlocks);
            Assert::IsTrue (largeDrive >= 0, L"the 32 MB volume is listed");

            EmulatorLink::Finish (sink, session->ReadBlockDevice (largeDrive, "C:\\blocks\\large.hdv", false), kVolumeMs);
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
