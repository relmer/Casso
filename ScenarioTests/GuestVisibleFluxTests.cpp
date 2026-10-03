#include "Pch.h"
#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "FluxTestImages.h"
#include "GuestSession.h"
#include "TestMachine.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Machines/Apple2/Common/NibblizationLayer.h"
#include "Machines/Apple2/Common/VolumeImage.h"
#include "Machines/Apple2/Common/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleFluxTests
//
//  DOS 3.3 on a disk made of flux tracks, as the real operating system sees
//  it.
//
//  Every track of the System Master is turned into flux with its cells
//  alternately 3% long and 3% short, the way a disk written on a drive that
//  ran fast and then slow comes out. DOS boots off that, saves a program to
//  it, and the image is flushed, reloaded and booted again to catalog the
//  program; then a file is put on it from outside the emulator and DOS loads
//  it. Our own decoder agreeing with our own encoder proves nothing here --
//  the witnesses are DOS's own CATALOG and BLOAD.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleFluxTests)
{
public:

    static constexpr int     kSlot          = 6;
    static constexpr int     kDrive         = 0;
    static constexpr int     kTracks        = NibblizationLayer::kTrackCount;
    static constexpr size_t  kStretchBits   = 3000;
    static constexpr double  kSpeedSpread   = 1.03;
    static constexpr size_t  kPayloadBytes  = 256;
    static constexpr Word    kLoadAddress   = 0x6000;

    static constexpr const char *  kHostFile   = "C:\\build\\prog.bin";
    static constexpr const char *  kPlacedName = "PROG";
    static constexpr const char *  kImagePath  = "flux.woz";



    //  The master as a WOZ 2.1 file of flux tracks only.
    static std::vector<Byte>  MakeFluxMaster (const std::vector<Byte> & master)
    {
        DiskImage                       bitImage;
        std::vector<WozSyntheticTrack>  tracks;
        std::vector<Byte>               woz;
        int                             t = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (master, bitImage),
            L"the master must nibblize");

        for (t = 0; t < kTracks; t++)
        {
            WozSyntheticTrack  track;
            size_t             count = bitImage.GetTrackBitCount (t);

            track.isFlux        = true;
            track.quarterTracks = { t * 4 };
            track.data          = FluxTestImages::BitsToFlux (bitImage.GetTrackBits (t), count,
                                      FluxTestImages::MakeAlternating (count, kStretchBits,
                                          FluxTestImages::kNominalCellTicks * kSpeedSpread,
                                          FluxTestImages::kNominalCellTicks / kSpeedSpread));
            tracks.push_back (track);
        }

        AssertSucceeded (WozLoader::BuildSyntheticV21 (tracks, woz), L"the flux WOZ must build");

        return woz;
    }



    //  What our own reader makes of a WOZ, used only as the pre-check before
    //  a processor starts. DOS is the witness that matters.
    static std::vector<Byte>  ReadSectors (const std::vector<Byte> & woz)
    {
        std::vector<Byte>   sectors;
        SectorDecodeReport  report;

        AssertSucceeded (VolumeImage::Load (woz, kImagePath, sectors, report),
            L"the flux WOZ must load as a volume");

        return sectors;
    }



    static void  AssertEveryTrackIsFlux (const std::vector<Byte> & woz)
    {
        DiskImage  image;
        int        t = 0;

        AssertSucceeded (WozLoader::Load (woz, image), L"the flushed WOZ must load");

        for (t = 0; t < kTracks; t++)
        {
            Assert::IsTrue (image.GetTrackKind (t) == TrackKind::Flux,
                std::format (L"track {} must still be a flux track after the save", t).c_str());
        }
    }



    static std::vector<Byte>  MakePayload()
    {
        std::vector<Byte>  bytes (kPayloadBytes, 0);
        size_t             i = 0;

        for (i = 0; i < kPayloadBytes; i++)
        {
            bytes[i] = (Byte) ((i * 11 + 0x3D) & 0xFF);
        }

        return bytes;
    }



    //  Puts a file on the WOZ the way the disk command does, from outside the
    //  emulator.
    static std::vector<Byte>  PutFile (const std::vector<Byte> & woz)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  options;
        DiskCommandResult   result;

        io.files[kImagePath]  = woz;
        io.stamps[kImagePath] = FileStamp { woz.size(), 100 };
        io.files[kHostFile]   = MakePayload();
        io.stamps[kHostFile]  = FileStamp { kPayloadBytes, 100 };

        options.subcommand          = CommandLineOptions::Subcommand::Disk;
        options.disk.command        = CommandLineOptions::DiskOptions::Command::Put;
        options.disk.imagePath      = kImagePath;
        options.disk.hostFile       = kHostFile;
        options.disk.path           = kPlacedName;
        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        {
            DiskCommandRunner  runner (io);

            result = runner.Run (options);
        }

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"putting a file on a flux disk must succeed");

        return io.files[kImagePath];
    }



    TEST_METHOD (Dos33BootsSavesAndCatalogsOnAFluxDisk)
    {
        TestMachine                machine ("Apple2e");
        std::vector<Byte>          master  = GuestSession::RequireDos33Master();
        std::vector<Byte>          woz     = MakeFluxMaster (master);
        std::vector<Byte>          flushed;
        std::vector<std::string>   rows;

        machine.GetDiskStore().SetFlushSink (
            [&flushed] (const std::string & path, const std::vector<Byte> & bytes) -> HRESULT
            {
                if (path == GuestSession::kpszWozGateName)
                {
                    flushed = bytes;
                }

                return S_OK;
            });

        GuestSession::BootWozToPrompt (machine, woz, master);

        rows = GuestSession::TypeAndCollect (machine, "10 PRINT \"FLUX\"");
        rows = GuestSession::TypeAndCollect (machine, "SAVE FLUXTEST");

        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"),
            L"DOS must save to a flux disk without an error");

        AssertSucceeded (machine.GetDiskStore().Flush (kSlot, kDrive), L"the flux disk must flush");

        Assert::IsFalse (flushed.empty(), L"the save must have reached the file");
        AssertEveryTrackIsFlux (flushed);

        GuestSession::BootWozToPrompt (machine, flushed, ReadSectors (flushed));

        rows = GuestSession::TypeAndCollect (machine, "CATALOG");

        Assert::IsTrue (GuestSession::AnyRowContains (rows, "FLUXTEST"),
            L"DOS must catalog the program it saved, after the flush and a fresh boot");
    }


    TEST_METHOD (Dos33LoadsAFilePutOnAFluxDiskFromOutside)
    {
        TestMachine                machine ("Apple2e");
        std::vector<Byte>          master  = GuestSession::RequireDos33Master();
        std::vector<Byte>          woz     = PutFile (MakeFluxMaster (master));
        std::vector<std::string>   rows;
        std::vector<Byte>          loaded;

        AssertEveryTrackIsFlux (woz);

        GuestSession::BootWozToPrompt (machine, woz, ReadSectors (woz));

        rows   = GuestSession::TypeAndCollect (machine, "BLOAD PROG");
        loaded = GuestSession::GuestBytesAt (machine, kLoadAddress, kPayloadBytes);

        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"),
            L"DOS must load a file written to a flux disk from outside");

        Assert::IsTrue (loaded == MakePayload(),
            L"and the bytes in memory must be the file's, all of them");
    }

    //  A flux disk made from nothing by `disk create --flux --bootable`, booted
    //  and written by DOS itself.
    TEST_METHOD (Dos33BootsAndSavesOnAFluxDiskMadeByCreate)
    {
        TestMachine                machine ("Apple2e");
        std::vector<Byte>          master  = GuestSession::RequireDos33Master();
        FakeDiskFileIo             io;
        DiskCommandRunner          runner  (io);
        CommandLineOptions         options;
        DiskCommandResult          result;
        std::vector<Byte>          woz;
        std::vector<std::string>   rows;

        io.files["master.dsk"]  = master;
        io.stamps["master.dsk"] = FileStamp { master.size(), 100 };

        options.subcommand        = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.commandWord  = "create";
        options.disk.imagePath    = kImagePath;
        options.disk.bootable     = true;
        options.disk.bootableFrom = "master.dsk";
        options.disk.flux         = true;

        result = runner.Run (options);
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus, L"create --flux --bootable must succeed");

        woz = io.files[kImagePath];
        AssertEveryTrackIsFlux (woz);

        GuestSession::BootWozToPrompt (machine, woz, ReadSectors (woz));

        rows = GuestSession::TypeAndCollect (machine, "10 PRINT \"MADE\"");
        rows = GuestSession::TypeAndCollect (machine, "SAVE MADEFLUX");

        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"),
            L"DOS must save to a flux disk create made");

        rows = GuestSession::TypeAndCollect (machine, "CATALOG");

        Assert::IsTrue (GuestSession::AnyRowContains (rows, "MADEFLUX"),
            L"and catalog what it saved");
    }
};
