#include "Pch.h"
#include "EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "GuestSession.h"
#include "HeadlessHost.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk2Controller.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GuestVisibleSharedImageTests
//
//  What a real 6502 makes of a disk that was rewritten underneath it.
//
//  EVERY OTHER TEST OF THIS FEATURE ASKS OUR OWN CODE WHETHER THE SWAP
//  HAPPENED, and our own code is exactly what cannot answer the question that
//  matters. The store can report a pick-up, refresh an identity and raise a
//  banner while the drive still reads the disk it was handed at mount -- the
//  emulator would look correct from every angle except the only one the
//  developer occupies. The witness here is the guest's own BLOAD.
//
//  IT IS ALSO THE ONE THAT CATCHES A DANGLING DRIVE. The controller is handed a
//  raw pointer to the mounted image, so a pick-up that replaced the image
//  OBJECT rather than its contents would leave the drive reading freed memory,
//  and nothing above the disk layer would notice.
//
//  A WRONG IMAGE MAKES THE PROCESSOR EXECUTE GARBAGE, which is slow, noisy and
//  says nothing useful, so the cheap questions come first here too.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GuestVisibleSharedImageTests)
{
public:

    static constexpr int   kSlot  = 6;
    static constexpr int   kDrive = 0;

    static constexpr const char *  kImagePath = "gate.dsk";
    static constexpr const char *  kHostFile  = "C:\\build\\prog.bin";
    static constexpr const char *  kPlacedName = "PROG";

    static constexpr size_t  kPayloadBytes  = 256;
    static constexpr Word    kLoadAddress   = 0x6000;



    //  Two builds of the same program, told apart by their bytes.
    static std::vector<Byte>  MakePayload (Byte seed)
    {
        std::vector<Byte>  bytes (kPayloadBytes, 0);
        size_t             i = 0;

        for (i = 0; i < kPayloadBytes; i++)
        {
            bytes[i] = (Byte) ((i * 7 + seed) & 0xFF);
        }

        return bytes;
    }



    //  Puts one build of the program onto the master, the way a developer's
    //  build step does.
    static std::vector<Byte>  PlaceBuild (const std::vector<Byte> & master, Byte seed)
    {
        FakeDiskFileIo      io;
        CommandLineOptions  options;
        DiskCommandResult   result;

        io.files[kImagePath]  = master;
        io.stamps[kImagePath] = FileStamp { master.size(), 100 };
        io.files[kHostFile]   = MakePayload (seed);
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
            L"the build step must succeed, or there is nothing to pick up");

        return io.files[kImagePath];
    }



    //  What the program's bytes read back as through the drive, asked before a
    //  processor is started: a mis-written image is reported here in
    //  milliseconds rather than by a 6502 executing whatever it loaded.
    static void  AssertTheDiskCarries (const std::vector<Byte> & image, Byte seed)
    {
        std::vector<Byte>  sectors = GuestSession::DecodeThroughTheDrive (image);
        Dos33Volume        volume (sectors);
        FilePayload        placed;

        AssertSucceeded (volume.Read (FilePath::Parse (kPlacedName), placed));

        Assert::IsTrue (placed.bytes == MakePayload (seed),
            L"the container the drive sees must carry this build of the program");
    }



    //  Types BLOAD and asserts which build landed in memory.
    static void  AssertGuestLoads (EmulatorCore & core, Byte seed)
    {
        std::vector<std::string>  rows;
        std::vector<Byte>         loaded;

        rows   = GuestSession::TypeAndCollect (core, "BLOAD PROG");
        loaded = GuestSession::GuestBytesAt (core, kLoadAddress, kPayloadBytes);

        Assert::IsFalse (GuestSession::AnyRowContains (rows, "ERROR"),
            L"the guest must not report an error loading the file");

        Assert::IsTrue (loaded == MakePayload (seed),
            L"and the bytes in memory must be this build's, all of them");
    }



    TEST_METHOD (AProgramRebuiltOutsideTheEmulatorIsLoadableByTheRunningGuest)
    {
        HeadlessHost       host;
        EmulatorCore       core;
        std::vector<Byte>  master  = GuestSession::RequireDos33Master();
        std::vector<Byte>  first   = PlaceBuild (master, 0x21);
        std::vector<Byte>  second  = PlaceBuild (master, 0x5C);
        ImageIdentity      identity;
        int64_t            nowMs   = 0;



        Assert::IsFalse (first == second, L"two builds must differ, or this proves nothing");

        AssertTheDiskCarries (first,  0x21);
        AssertTheDiskCarries (second, 0x5C);

        GuestSession::BootToPrompt (host, core, first);

        //  The developer's first run: the program they built is on the disk in
        //  the drive, and the guest loads it.
        AssertGuestLoads (core, 0x21);

        //  Now the build step runs again, over the mounted image. The reader
        //  seam stands in for the file, which is what lets the whole path be
        //  driven without a disk on the host.
        core.diskStore->SetImageReader (
            [&second] (const std::string &, std::vector<Byte> & bytes) -> HRESULT
            {
                bytes = second;

                return S_OK;
            });

        identity.recorded           = true;
        identity.stamp.sizeBytes    = second.size();
        identity.stamp.modifiedUnix = 999;

        core.diskStore->SetIdentityReader (
            [&identity] (const std::string &) { return identity; });

        //  The clock goes in BEFORE the change is noted, since the quiet period
        //  is measured between two readings of the same clock.
        core.diskStore->SetClock ([&nowMs] () { return nowMs; });

        //  Stated by the writer, which is what a `--on-change reload` build
        //  step says: take the new contents, leave the machine running.
        core.diskStore->NoteExternalChange (kImagePath, PickUpIntent::TakeUpInPlace);

        //  Time passes and the machine reaches a quiet moment -- the two things
        //  the emulator's own clock and idle callback supply in a real session.
        nowMs += MountedImageState::kQuietPeriodMs;

        core.diskStore->ApplyPendingPickUp();

        //  SC-001: the guest can run the new program, with no eject and no
        //  re-insert by hand. Nothing below the disk layer was told anything --
        //  the drive is still reading the same DiskImage it was handed at
        //  mount, whose contents changed underneath it.
        AssertGuestLoads (core, 0x5C);
    }
};
