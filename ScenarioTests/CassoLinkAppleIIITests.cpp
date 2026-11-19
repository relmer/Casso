#include "Pch.h"

#include "EhmTestHelper.h"
#include "EmulatorLink.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "CassoLink/CassoLinkSession.h"
#include "CassoLink/CassoLinkSosDisk.h"
#include "CassoLink/InMemoryCassoLinkFolder.h"
#include "CassoLink/Session/CassoLinkBlockDevices.h"
#include "Core/TextEncoding.h"
#include "Devices/Disk/FilePath.h"
#include "Machines/Apple2/Common/ProDosSkeleton.h"
#include "Machines/Apple2/Common/ProDosVolume.h"
#include "Seams/Win32ByteStreamFactory.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CassoLinkAppleIIITests
//
//  Story 14's operations on an Apple /// in MAME (SC-059): the CassoLink SOS
//  disk made from the user's SOS disk boots in MAME's apple3, headless, with
//  its serial port on a TCP socket to a Wi-Fi modem connection, which
//  listens. A blank ProDOS disk is in the external drive. The Apple /// is
//  listed with both drives, a file is copied each way, a file renamed and
//  deleted and a folder made, and each disk read to an image and an image
//  written to the external drive.
//
//  The SOS disk is Apple's System Demo disk, the stock disk whose SOS.DRIVER
//  has .RS232. Run only when CASSO_MAME_APPLE3 is set and MAME is installed
//  with its apple3 ROMs; it passes with nothing done otherwise. The link runs
//  at 9,600 baud, as MAME's null modem does by default, and each whole disk
//  takes about two and a half minutes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (CassoLinkAppleIIITests)
{
public:

    static constexpr uint16_t      kPort        = 19801;
    static constexpr DWORD         kBaud        = 9600;
    static constexpr DWORD         kConnectMs   = 120'000;
    static constexpr DWORD         kWholeDiskMs = 600'000;
    static constexpr DWORD         kFileMs      = 180'000;
    static constexpr size_t        kSector      = 256;
    static constexpr size_t        kBlock       = 512;
    static constexpr size_t        kBlocks      = 280;
    static constexpr size_t        kTrackBlocks = 8;
    static constexpr int           kDosSector[] = { 0, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 15 };
    static constexpr const char *  kD1Image     = "C:\\a3\\d1.po";
    static constexpr const char *  kD2Image     = "C:\\a3\\d2.po";
    static constexpr const char *  kD2Written   = "C:\\a3\\d2-written.po";



    //  A 140K disk's sectors in DOS order, as a .dsk has them, in ProDOS
    //  block order, as a block device's image has them.
    static std::vector<Byte>  ToBlockOrder (const std::vector<Byte> & dosOrder)
    {
        std::vector<Byte>  out (kBlocks * kBlock, 0);



        for (size_t block = 0; block < kBlocks; block++)
        {
            size_t  track = block / kTrackBlocks;

            for (size_t half = 0; half < 2; half++)
            {
                size_t  from = (track * 16 + (size_t) kDosSector[(block % kTrackBlocks) * 2 + half]) * kSector;

                std::copy_n (dosOrder.begin() + (ptrdiff_t) from, kSector, out.begin() + (ptrdiff_t) ((block * 2 + half) * kSector));
            }
        }

        return out;
    }




    TEST_METHOD (AnAppleIIIInMame_RunsStory14sOperations)
    {
        std::string                        run     = EmulatorLink::GetSetting ("CASSO_MAME_APPLE3");
        std::filesystem::path              mame    = std::filesystem::path (EmulatorLink::GetSetting ("LOCALAPPDATA")) / "Programs" / "MAME" / "mame.exe";
        std::filesystem::path              folder  = std::filesystem::temp_directory_path() / "CassoAppleIII";
        std::vector<Byte>                  sosDisk;
        std::vector<Byte>                  linkDisk;
        std::vector<Byte>                  blank (kBlocks * kBlock, 0);
        std::vector<Byte>                  toWrite (kBlocks * kBlock, 0);
        std::vector<Byte>                  next;
        std::string                        error;
        FilePayload                        kernel;
        FilePayload                        copied  = EmulatorLink::MakeBinary (3000, 5);
        FilePayload                        onDisk  = EmulatorLink::MakeBinary (1500, 9);
        Win32ByteStreamFactory             factory;
        InMemoryCassoLinkFolder            linkFolder;
        FakeDiskFileIo                     fileIo;
        EmulatorLink::Sink                 sink;
        std::unique_ptr<CassoLinkSession>  session;
        CassoLinkSettings                  settings;
        EmulatorLink::Process              emulator;
        int                                d1      = CassoLinkBlockDevices::kFirstDrive;
        int                                d2      = CassoLinkBlockDevices::kFirstDrive + 1;



        if (run.empty())
        {
            Logger::WriteMessage ("NOT RUN: set CASSO_MAME_APPLE3 to run the Apple /// in MAME\n");
            return;
        }

        Assert::IsTrue (std::filesystem::exists (mame), L"MAME is installed");

        //  The link disk, a blank external disk, and the image written to
        //  the external drive: a ProDOS volume with a file.
        sosDisk = GuestSession::RequireSosDemoDisk();
        AssertSucceeded (CassoLinkSosDisk::Make (sosDisk, "sos-demo.dsk", kBaud, linkDisk, error));
        AssertSucceeded (ProDosSkeleton::Write (blank, "EXTERNAL"));
        AssertSucceeded (ProDosSkeleton::Write (toWrite, "WRITTEN"));
        AssertSucceeded (ProDosVolume (toWrite).Write (FilePath::Parse ("ONDISK"), onDisk, next));
        toWrite = std::move (next);
        AssertSucceeded (ProDosVolume (linkDisk).Read (FilePath::Parse ("SOS.KERNEL"), kernel));

        std::filesystem::create_directories (folder);
        EmulatorLink::WriteHostFile (folder / "d1.dsk", linkDisk);
        EmulatorLink::WriteHostFile (folder / "d2.dsk", blank);

        fileIo.files[kD2Written]  = ToBlockOrder (toWrite);
        fileIo.stamps[kD2Written] = FileStamp { toWrite.size(), 1 };

        //  The connection listens; MAME connects to it once it starts.
        settings.name      = "Apple3";
        settings.transport = LinkTransport::WifiModem;
        settings.address   = std::format ("127.0.0.1:{}", kPort);
        settings.baud      = kBaud;

        session = std::make_unique<CassoLinkSession> (factory, linkFolder, fileIo, sink);
        session->SetConnectTimeout (kConnectMs);
        AssertSucceeded (session->Start (settings));

        emulator.Start (std::format (L"\"{}\" apple3 -flop1 \"{}\" -flop2 \"{}\" -rs232 null_modem -bitb socket.127.0.0.1:{} "
                                     L"-video none -sound none -skip_gameinfo -seconds_to_run 3600",
                                     mame.wstring(), (folder / "d1.dsk").wstring(), (folder / "d2.dsk").wstring(), kPort),
                        mame.parent_path());

        Assert::IsTrue (sink.WaitForState (LinkState::Ready, kConnectMs), L"the Apple /// connects");
        Assert::AreEqual (std::wstring (L"Apple ///"), session->GetModelName());

        //  The root: the built-in and external drives.
        EmulatorLink::Finish (sink, session->ListBlockDevices(), kFileMs);

        std::vector<CassoLinkBlockDevice>  devices = session->GetBlockDevices();

        Assert::IsTrue (devices.size() >= 2, L"the built-in and external drives");
        Assert::AreEqual (std::string (".D1"),       devices[0].device);
        Assert::AreEqual (std::string ("CASSOLINK"), devices[0].volume);
        Assert::AreEqual (std::string (".D2"),       devices[1].device);
        Assert::AreEqual (std::string ("EXTERNAL"),  devices[1].volume);
        Assert::AreEqual ((uint32_t) kBlocks,        devices[0].blocks);

        //  Browse, and a file copied from the Apple.
        auto  catalog = EmulatorLink::Finish (sink, session->Enumerate (d1, FilePath()), kFileMs);
        auto  files   = EmulatorLink::GetNames (catalog->listing);

        for (const std::string & file : files)
        {
            Logger::WriteMessage (("on .D1: " + file + "\n").c_str());
        }

        Assert::IsTrue (std::find (files.begin(), files.end(), "SOS.KERNEL") != files.end(), TextEncoding::NarrowToWide (".D1 has: " + std::accumulate (files.begin(), files.end(), std::string(), [] (std::string all, const std::string & file) { return all + file + " "; })).c_str());
        Assert::IsTrue (std::find (files.begin(), files.end(), "SOS.INTERP") != files.end(), L"and SOS.INTERP");

        auto  read = EmulatorLink::Finish (sink, session->ReadFile (d1, FilePath::Parse ("SOS.KERNEL")), kFileMs);

        Assert::IsTrue (read->file.bytes == kernel.bytes, L"SOS.KERNEL copied from the Apple is the disk's");

        //  A file copied to the Apple, read back, renamed and deleted, and a
        //  folder made.
        EmulatorLink::Finish (sink, session->WriteFile (d2, FilePath::Parse ("COPIED"), copied), kFileMs);

        auto  back = EmulatorLink::Finish (sink, session->ReadFile (d2, FilePath::Parse ("COPIED")), kFileMs);

        Assert::IsTrue (back->file.bytes == copied.bytes, L"the file copied to the Apple is read back");

        EmulatorLink::Finish (sink, session->Rename (d2, FilePath::Parse ("COPIED"), "RENAMED"), kFileMs);
        EmulatorLink::Finish (sink, session->MakeDirectory (d2, FilePath::Parse ("FOLDER")), kFileMs);
        EmulatorLink::Finish (sink, session->Delete (d2, FilePath::Parse ("RENAMED")), kFileMs);

        catalog = EmulatorLink::Finish (sink, session->Enumerate (d2, FilePath()), kFileMs);
        Assert::IsTrue (EmulatorLink::GetNames (catalog->listing) == std::vector<std::string> { "FOLDER" }, L".D2 has the folder alone");

        //  Each disk read to an image, and an image written to the external
        //  drive and read back.
        EmulatorLink::Finish (sink, session->ReadBlockDevice (d1, kD1Image, false), kWholeDiskMs);
        Assert::IsTrue (fileIo.files[kD1Image] == ToBlockOrder (linkDisk), L"the built-in drive's image is the link disk");

        EmulatorLink::Finish (sink, session->WriteBlockDevice (d2, kD2Written, false), kWholeDiskMs);
        EmulatorLink::Finish (sink, session->ReadBlockDevice (d2, kD2Image, false), kWholeDiskMs);
        Assert::IsTrue (fileIo.files[kD2Image] == fileIo.files[kD2Written], L"the external drive has the image written to it");

        session.reset();
    }
};
