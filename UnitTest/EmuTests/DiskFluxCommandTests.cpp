#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFormat.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFluxCommandTests
//
//  disk create and init with --flux: which tracks of a WOZ are flux.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskFluxCommandTests)
{
public:

    static int  CountFluxTracks (const vector<Byte> & woz)
    {
        DiskImage  image;
        int        count = 0;
        int        track = 0;
        int        slot  = 0;

        AssertSucceeded (WozLoader::Load (woz, image), L"the WOZ must load");

        for (track = 0; track < NibbleFormat::kTrackCount; track++)
        {
            slot = image.ResolveQuarterTrack (track * DiskImage::kQuarterTracksPerWholeTrack);

            if (slot >= 0 && image.GetTrackKind (slot) == TrackKind::Flux)
            {
                count++;
            }
        }

        return count;
    }

    TEST_METHOD (CreateFlux_ListedTracksAreFluxAndTheSummarySaysWhich)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner  (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("f.woz");
        DiskCommandResult   result;

        options.disk.flux       = true;
        options.disk.fluxTracks = "0-2,17";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (4, CountFluxTracks (io.files["f.woz"]));
        Assert::IsTrue (result.output.find (", flux tracks 0-2, 17") != std::string::npos);
    }

    TEST_METHOD (CreateFlux_BareIsEveryTrack)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner  (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("f.woz");
        DiskCommandResult   result;

        options.disk.flux = true;
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (NibbleFormat::kTrackCount, CountFluxTracks (io.files["f.woz"]));
    }

    TEST_METHOD (CreateFlux_ABadTrackListIsRefused)
    {
        const char *  bad[] = { "35", "3-1", "1,", "-2", "1..3", "a" };

        for (const char * list : bad)
        {
            FakeDiskFileIo      io;
            DiskCommandRunner   runner  (io);
            CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("f.woz");
            DiskCommandResult   result;

            options.disk.flux       = true;
            options.disk.fluxTracks = list;
            result                  = runner.Run (options);

            Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
            Assert::IsTrue (result.diagnostics.find ("illegal track list") != std::string::npos);
            Assert::IsFalse (io.Exists ("f.woz"));
        }
    }

    TEST_METHOD (CreateFlux_ANonWozContainerIsRefused)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner  (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeCreate ("f.dsk");
        DiskCommandResult   result;

        options.disk.flux = true;
        result            = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("flux tracks need a WOZ image") != std::string::npos);
        Assert::IsFalse (io.Exists ("f.dsk"));
    }

    TEST_METHOD (InitFlux_KeepsTheTracksThatWereFlux)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner  (io);
        CommandLineOptions  create  = DiskRunnerFixtures::MakeCreate ("f.woz");
        CommandLineOptions  init    = DiskRunnerFixtures::MakeCreate ("f.woz");
        DiskCommandResult   result;

        create.disk.flux       = true;
        create.disk.fluxTracks = "5-7";
        runner.Run (create);

        init.disk.command     = CommandLineOptions::DiskOptions::Command::Init;
        init.disk.commandWord = "init";
        init.disk.formatName  = "prodos";
        result                = runner.Run (init);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (3, CountFluxTracks (io.files["f.woz"]), L"reformatting keeps the flux tracks");
    }

    TEST_METHOD (CreateFlux_ADirectBootDiskCanBeFluxToo)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = DiskRunnerFixtures::MakeDirectBoot ("boot.woz", "prog.bin");

        io.files["prog.bin"]    = vector<Byte> (64, (Byte) 0xEA);
        options.disk.flux       = true;
        options.disk.fluxTracks = "0";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);
        Assert::AreEqual (1, CountFluxTracks (io.files["boot.woz"]));

        options                 = DiskRunnerFixtures::MakeDirectBoot ("boot.dsk", "prog.bin");
        options.disk.flux       = true;

        Assert::AreEqual (DiskCommandResult::kNoOutput, runner.Run (options).exitStatus,
                          L"and a direct-boot disk that is not a WOZ is refused");
    }
};
