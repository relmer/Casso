#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/SectorDecodeReport.h"
#include "Devices/Disk/VolumeImage.h"
#include "Devices/Disk/WozLoader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskFailureModeTests
//
//  One question asked the same way of every documented failure a write can
//  suffer: a volume with no room, a locked file, a write-protected image, a
//  name the catalog cannot store, a track that cannot be rewritten, a target
//  that moved since it was read, and a target another program holds open.
//
//  EVERY CASE ASKS THREE THINGS, AND ASKING ONLY THE FIRST IS THE TRAP. A
//  refusal that hands back an error is satisfied by a half-written image, so
//  each case asserts the target's bytes are what they were, that the image
//  still parses as a mountable volume, and that nothing was left in the file
//  table. The second is what separates a comparison of two disks from a
//  comparison of two piles of garbage: it is the assertion that fails when the
//  material a case is built on stops being a volume, which byte equality
//  against a rebuilt oracle cannot see.
//
//  Nothing here touches a real file. The seam is what makes the suite legal at
//  all, and the one thing it cannot cover -- a process killed mid-commit -- is
//  the reason a manual pass exists beside it.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskFailureModeTests)
{
public:

    static constexpr const char *  kRealImage = "C:\\disks\\merlin.dsk";
    static constexpr const char *  kBlankDsk  = "C:\\disks\\blank.dsk";
    static constexpr const char *  kBlankWoz  = "C:\\disks\\blank.woz";
    static constexpr const char *  kHostFile  = "C:\\build\\prog.bin";

    static constexpr const char *  kRealFixture = "Disks/Merlin-proDos2.23.dsk";

    //  A file that is really on the vendor's disk, so the locked cases run
    //  against a catalog entry nobody here invented.
    static constexpr const char *  kRealFile = "MAKE DUMP";

    //  The same name as the listing renders it. The trailing newline is load
    //  bearing: this disk also carries MAKE DUMP.S, and a bare substring search
    //  goes on finding the file inside its neighbor's name after it is gone.
    static constexpr const char *  kRealFileListed = "MAKE DUMP\n";

    //  What a freshly formatted volume says about itself, which is how the
    //  blank cases prove the image still mounts.
    static constexpr const char *  kBlankVolumeListed = "DISK VOLUME 254";

    static constexpr Byte    kBlankVolumeNumber = 254;
    static constexpr size_t  kPayloadBytes      = 512;
    static constexpr Word    kLoadAddress       = 0x6000;

    //  Larger than the 496 sectors a freshly formatted volume leaves free, so
    //  the placement is refused for want of room and not for anything else.
    static constexpr size_t  kOversizedBytes    = 130000;

    //  A payload pattern no neighboring sector could be mistaken for.
    static constexpr size_t  kPayloadStride = 7;
    static constexpr size_t  kPayloadSeed   = 0x21;
    static constexpr size_t  kByteMask      = 0xFF;

    //  DOS 3.3 catalog geometry, restated only for the helper that locks a
    //  named entry -- the one place these tests reach into the format.
    static constexpr int     kVtocTrack          = 17;
    static constexpr int     kCatalogFirstSector = 15;
    static constexpr int     kEntriesPerSector   = 7;

    static constexpr size_t  kSecOffNextTrack  = 0x01;
    static constexpr size_t  kSecOffNextSector = 0x02;
    static constexpr size_t  kEntryBase        = 0x0B;
    static constexpr size_t  kEntryStride      = 0x23;
    static constexpr size_t  kEntOffType       = 0x02;
    static constexpr size_t  kEntOffName       = 0x03;
    static constexpr size_t  kNameLength       = 30;

    static constexpr Byte    kLockedBit   = 0x80;
    static constexpr Byte    kHighBit     = 0x80;
    static constexpr Byte    kNamePadding = 0xA0;

    //  The track carrying the catalog and the free map. The unwritable-track
    //  case must damage something else, or the volume stops being readable and
    //  the case stops being the one intended.
    static constexpr int     kCatalogTrack = 17;

    //  How much of a track's bit stream to erase so it loses some but not all
    //  of its sectors. A wholly blank track is Unformatted and writable; this
    //  leaves address fields standing in the surviving half, which is what
    //  makes the outcome Partial.
    static constexpr size_t  kErasedFraction = 2;


    //
    //  ------------------------------------------------------------------
    //  The three questions.
    //  ------------------------------------------------------------------
    //

    //  Byte for byte against an oracle the failing operation could not have
    //  reached -- read again from the fixture on disk, or rebuilt by a pure
    //  function, never lifted out of the fake after the fact.
    void AssertImageIsTheOracle (FakeDiskFileIo      & io,
                                 const char          * imagePath,
                                 const vector<Byte>  & oracle)
    {
        size_t  i = 0;

        Assert::IsTrue (oracle.size() > 0, L"the oracle must actually hold an image");

        Assert::AreEqual (size_t (1), io.files.count (imagePath),
            L"the image must still be there at all");

        Assert::AreEqual (oracle.size(), io.files[imagePath].size(),
            L"and be the size it was");

        for (i = 0; i < oracle.size(); i++)
        {
            if (io.files[imagePath][i] != oracle[i])
            {
                Assert::Fail (L"the image differs from what it was before the refusal");
            }
        }
    }

    //  Still a volume something could mount: it loads, its filesystem is
    //  recognized, its catalog enumerates, and what it carried is still in it.
    void AssertImageStillMounts (FakeDiskFileIo  & io,
                                 const char      * imagePath,
                                 const char      * mustStillList)
    {
        DiskCommandRunner   reader (io);
        CommandLineOptions  options;
        DiskCommandResult   listing;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::List;
        options.disk.imagePath = imagePath;

        listing = reader.Run (options);

        Assert::AreNotEqual (DiskCommandRunner::kNoOutput, listing.exitStatus,
            L"the image must still mount -- a refusal that leaves it unreadable is worse "
            L"than the operation it refused");

        Assert::IsTrue (listing.output.find (" free of ") != std::string::npos,
            L"and still account for its own free space");

        Assert::IsTrue (listing.output.find (mustStillList) != std::string::npos,
            L"and still carry what it carried before");
    }

    //  Nothing beside the image but what the test put there. HasNoTemporaryFiles
    //  answers for the name this tool derives; comparing the whole table answers
    //  for anything left under a name it does not.
    void AssertNoStrayFileRemains (FakeDiskFileIo & io, const vector<std::string> & expected)
    {
        Assert::IsTrue (io.HasNoTemporaryFiles(),
            L"no temporary may be left beside the image");

        Assert::AreEqual (expected.size(), io.files.size(),
            L"and nothing may be left under a name the temporary sweep does not know");

        for (const std::string & path : expected)
        {
            Assert::AreEqual (size_t (1), io.files.count (path),
                L"every file the test seeded must still be there");
        }
    }

    //  Asked as one call so a case cannot answer two of the three and look
    //  complete.
    void AssertRefusalLeftEverythingAsItWas (FakeDiskFileIo             & io,
                                             const char                 * imagePath,
                                             const vector<Byte>         & oracle,
                                             const char                 * mustStillList,
                                             const vector<std::string>  & expectedFiles)
    {
        AssertImageIsTheOracle   (io, imagePath, oracle);
        AssertImageStillMounts   (io, imagePath, mustStillList);
        AssertNoStrayFileRemains (io, expectedFiles);
    }

    //  A refusal has to read as a reason. A hexadecimal code is the shape a
    //  message picks up the moment somebody formats an HRESULT into it.
    static void AssertNamesNoPlatformCode (const std::string & diagnostics)
    {
        Assert::IsTrue (diagnostics.find ("0x") == std::string::npos,
            L"a refusal must not carry a raw platform code");
    }

    //  And it names ONE reason.
    //
    //  A second line means the operation reported its cause and then carried on
    //  far enough to trip over something else, which leaves the reader choosing
    //  between two candidate explanations for one refusal -- and leaves the
    //  stopping point to whatever happened to fail next rather than to the
    //  refusal that was supposed to stop it.
    static void AssertNamesOneReason (const std::string & diagnostics)
    {
        size_t  lines = 0;
        size_t  at    = 0;

        while ((at = diagnostics.find ('\n', at)) != std::string::npos)
        {
            lines++;
            at++;
        }

        Assert::AreEqual (size_t (1), lines, L"a refusal must name one reason, not two");
    }


    //
    //  ------------------------------------------------------------------
    //  Material. Every builder answers the same bytes every time it is
    //  called, so one call seeds the fake and another produces the oracle
    //  without the two sharing anything the operation under test can reach.
    //  ------------------------------------------------------------------
    //

    vector<Byte> RealDiskBytes()
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture (kRealFixture, bytes));
        Assert::IsTrue (bytes.size() > 0, L"the fixture must actually have been read");

        return bytes;
    }

    vector<Byte> BlankDos33Sectors()
    {
        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kBlankVolumeNumber));

        return buffer;
    }

    vector<Byte> MakePayload (size_t count = kPayloadBytes)
    {
        vector<Byte>  bytes (count, 0);
        size_t        i     = 0;

        for (i = 0; i < count; i++)
        {
            bytes[i] = (Byte) ((i * kPayloadStride + kPayloadSeed) & kByteMask);
        }

        return bytes;
    }

    void SeedFile (FakeDiskFileIo & io, const char * path, const vector<Byte> & bytes)
    {
        io.files[path]  = bytes;
        io.stamps[path] = FileStamp { bytes.size(), 100 };
    }

    CommandLineOptions MakePutOptions (const char * image, const char * asName)
    {
        CommandLineOptions  options;

        options.subcommand          = CommandLineOptions::Subcommand::Disk;
        options.disk.command           = CommandLineOptions::DiskOptions::Command::Put;
        options.disk.imagePath      = image;
        options.disk.hostFile       = kHostFile;
        options.disk.path           = asName;
        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        return options;
    }

    //  The same placement as a text file.
    //
    //  THE FULL-VOLUME CASE NEEDS THIS. A binary records its length in two
    //  bytes, so an oversized one is refused for being longer than the
    //  filesystem can record before the volume's room is ever consulted -- a
    //  perfectly good refusal, and not the one this gate is asking about.
    CommandLineOptions MakeTextPutOptions (const char * image, const char * asName)
    {
        CommandLineOptions  options = MakePutOptions (image, asName);

        options.disk.typeName       = "T";
        options.disk.hasLoadAddress = false;

        return options;
    }

    CommandLineOptions MakeDeleteOptions (const char * image, const char * path)
    {
        CommandLineOptions  options;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::Delete;
        options.disk.imagePath = image;
        options.disk.path      = path;

        return options;
    }

    //  Sets the lock bit on the entry with the given name, walking the catalog
    //  chain the way the reader does. By name rather than by position, because
    //  the first slot on this disk is a decorative heading that owns no
    //  sectors, and a rule about locked files proves little against one.
    static bool TryLockCatalogEntryNamed (vector<Byte> & buffer, const std::string & name)
    {
        int     track    = kVtocTrack;
        int     sector   = kCatalogFirstSector;
        int     slot     = 0;
        size_t  base     = 0;
        size_t  at       = 0;
        size_t  i        = 0;
        Byte    stored   = 0;
        Byte    expected = 0;
        bool    same     = false;

        while (track != 0)
        {
            base = Dos33Skeleton::SectorOffset (track, sector);

            for (slot = 0; slot < kEntriesPerSector; slot++)
            {
                at   = base + kEntryBase + (kEntryStride * slot);
                same = true;

                for (i = 0; i < kNameLength; i++)
                {
                    stored   = (Byte) (buffer[at + kEntOffName + i] & ~kHighBit);
                    expected = (i < name.size())
                             ? (Byte) name[i]
                             : (Byte) (kNamePadding & ~kHighBit);

                    if (stored != expected)
                    {
                        same = false;
                    }
                }

                if (same)
                {
                    buffer[at + kEntOffType] = (Byte) (buffer[at + kEntOffType] | kLockedBit);

                    return true;
                }
            }

            track  = buffer[base + kSecOffNextTrack];
            sector = buffer[base + kSecOffNextSector];
        }

        return false;
    }

    //  The vendor's disk with one of its real files locked, rebuilt from the
    //  fixture every time it is asked for.
    vector<Byte> RealDiskWithALockedFile()
    {
        vector<Byte>  bytes  = RealDiskBytes();
        bool          locked = TryLockCatalogEntryNamed (bytes, kRealFile);

        Assert::IsTrue (locked, L"the file the locked cases run against must be on the disk");

        return bytes;
    }

    vector<Byte> MakeWozOf (const vector<Byte> & sectors)
    {
        DiskImage     image;
        vector<Byte>  woz;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));
        AssertSucceeded (WozLoader::Serialize (image, woz));

        return woz;
    }

    //  A bit-stream image with part of one track's stream erased, which costs
    //  that track some of its sectors and costs no other track any of its own.
    vector<Byte> MakeWozWithAHalfErasedTrack (const vector<Byte> & sectors, int track)
    {
        DiskImage     image;
        vector<Byte>  woz;
        size_t        from  = 0;
        size_t        i     = 0;

        AssertSucceeded (NibblizationLayer::NibblizeDsk (sectors, image));

        {
            vector<Byte> &  bits = image.GetTrackBitsForWrite (track);

            from = bits.size() / kErasedFraction;

            Assert::IsTrue (from > 0, L"the track must carry bits to erase");

            for (i = from; i < bits.size(); i++)
            {
                bits[i] = 0;
            }
        }

        AssertSucceeded (WozLoader::Serialize (image, woz));

        return woz;
    }


    //
    //  ------------------------------------------------------------------
    //  The failure modes.
    //  ------------------------------------------------------------------
    //

    TEST_METHOD (AVolumeWithNoRoom_RefusesThePutAndLeavesAMountableImageUnchanged)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedFile (io, kBlankDsk, BlankDos33Sectors());
        SeedFile (io, kHostFile, MakePayload (kOversizedBytes));

        result = runner.Run (MakeTextPutOptions (kBlankDsk, "BIG"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("does not fit") != std::string::npos,
            L"the reason must be the volume's room and not something generic");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertNamesOneReason      (result.diagnostics);

        AssertRefusalLeftEverythingAsItWas (io, kBlankDsk, BlankDos33Sectors(),
                                            kBlankVolumeListed,
                                            { kBlankDsk, kHostFile });
    }

    TEST_METHOD (ALockedFile_RefusesThePutAndLeavesAMountableImageUnchanged)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedFile (io, kRealImage, RealDiskWithALockedFile());
        SeedFile (io, kHostFile,  MakePayload());

        result = runner.Run (MakePutOptions (kRealImage, kRealFile));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("is locked on this volume") != std::string::npos,
            L"the reason must be the lock, not merely that something failed");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertNamesOneReason      (result.diagnostics);

        AssertRefusalLeftEverythingAsItWas (io, kRealImage, RealDiskWithALockedFile(),
                                            kRealFileListed,
                                            { kRealImage, kHostFile });
    }

    TEST_METHOD (ALockedFile_RefusesTheDeleteAndLeavesAMountableImageUnchanged)
    {
        // Removal travels its own path to the same commit, so a bail-out
        // deleted from one of the two is invisible to a gate that only puts.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedFile (io, kRealImage, RealDiskWithALockedFile());

        result = runner.Run (MakeDeleteOptions (kRealImage, kRealFile));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("is locked on this volume") != std::string::npos,
            L"the reason must be the lock");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertNamesOneReason      (result.diagnostics);

        AssertRefusalLeftEverythingAsItWas (io, kRealImage, RealDiskWithALockedFile(),
                                            kRealFileListed,
                                            { kRealImage });
    }

    TEST_METHOD (AWriteProtectedImage_RefusesThePutAndLeavesAMountableImageUnchanged)
    {
        // The refusal that arrives LAST. Nothing about the image's contents
        // says it may not be written, so the volume computes a perfectly good
        // new image and the platform denies access at the final step -- with a
        // complete temporary already sitting beside the target, which is the
        // most tempting moment to leave one behind.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedFile (io, kBlankDsk, BlankDos33Sectors());
        SeedFile (io, kHostFile, MakePayload());

        io.failNextReplace  = true;
        io.nextReplaceError = HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);

        result = runner.Run (MakePutOptions (kBlankDsk, "PROG"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("write-protected") != std::string::npos,
            L"the reason must be write protection, not a generic replace failure");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertNamesOneReason      (result.diagnostics);

        Assert::AreEqual (1, io.replaceCount, L"and the replace really was reached");

        AssertRefusalLeftEverythingAsItWas (io, kBlankDsk, BlankDos33Sectors(),
                                            kBlankVolumeListed,
                                            { kBlankDsk, kHostFile });
    }

    TEST_METHOD (ANameTheCatalogCannotStore_RefusesThePutAndLeavesAMountableImageUnchanged)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedFile (io, kBlankDsk, BlankDos33Sectors());
        SeedFile (io, kHostFile, MakePayload());

        result = runner.Run (MakePutOptions (kBlankDsk, "9LIVES"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("9LIVES") != std::string::npos,
            L"the message names the name it refused");
        Assert::IsTrue (result.diagnostics.find ("starting with a letter") != std::string::npos,
            L"and says what a legal one looks like");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertNamesOneReason      (result.diagnostics);

        AssertRefusalLeftEverythingAsItWas (io, kBlankDsk, BlankDos33Sectors(),
                                            kBlankVolumeListed,
                                            { kBlankDsk, kHostFile });
    }

    TEST_METHOD (AnUnwritableTrackThePutNeeds_RefusesItAndLeavesAMountableImageUnchanged)
    {
        // Two passes, because the track to damage has to be one the write
        // GENUINELY NEEDS. Damaging one first would change where the allocator
        // puts the file, and the case would stop being the case.
        FakeDiskFileIo      survey;
        FakeDiskFileIo      io;
        DiskCommandRunner   surveyor (survey);
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        SectorDecodeReport  report;
        vector<Byte>        sectors = BlankDos33Sectors();
        vector<Byte>        clean   = MakeWozOf (sectors);
        vector<Byte>        damaged;
        vector<Byte>        before;
        vector<Byte>        after;
        vector<int>         changed;
        int                 victim  = -1;
        size_t              i       = 0;
        bool                spans   = false;

        SeedFile (survey, kBlankWoz, clean);
        SeedFile (survey, kHostFile, MakePayload());

        Assert::AreEqual (DiskCommandRunner::kClean,
                          surveyor.Run (MakePutOptions (kBlankWoz, "PROG")).exitStatus,
            L"the survey pass must succeed, or it says nothing about which tracks are needed");

        AssertSucceeded (VolumeImage::Load (clean, kBlankWoz, before, report));
        AssertSucceeded (VolumeImage::Load (survey.files[kBlankWoz], kBlankWoz, after, report));

        VolumeImage::ChangedTracks (before, after, changed);

        for (i = 0; i < changed.size(); i++)
        {
            if (changed[i] != kCatalogTrack && victim < 0)
            {
                victim = changed[i];
            }
        }

        Assert::IsTrue (victim >= 0, L"the write must land somewhere besides the catalog track");

        for (i = 0; i < changed.size(); i++)
        {
            if (changed[i] != victim)
            {
                spans = true;
            }
        }

        Assert::IsTrue (spans,
            L"at least one other track must be needed and writable, so that skipping the "
            L"damaged one would have produced a file rather than nothing");

        damaged = MakeWozWithAHalfErasedTrack (sectors, victim);

        AssertSucceeded (VolumeImage::Load (damaged, kBlankWoz, before, report));

        Assert::IsTrue (TrackDecodeOutcome::Partial == report.GetOutcome (victim),
            L"the damaged track must decode Partial, or this is not the case intended");

        SeedFile (io, kBlankWoz, damaged);
        SeedFile (io, kHostFile, MakePayload());

        result = runner.Run (MakePutOptions (kBlankWoz, "PROG"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("track " + std::to_string (victim))
                            != std::string::npos,
            L"the refusal must name the track it is about");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertNamesOneReason      (result.diagnostics);

        AssertRefusalLeftEverythingAsItWas (io, kBlankWoz,
                                            MakeWozWithAHalfErasedTrack (sectors, victim),
                                            kBlankVolumeListed,
                                            { kBlankWoz, kHostFile });
    }

    TEST_METHOD (ATargetThatMovedSinceItWasRead_RefusesThePutAndLeavesAMountableImageUnchanged)
    {
        // The fake perturbs the stamp RECORDED at read rather than the one
        // observed at commit, because the whole command runs in one call and the
        // read is the first stat it makes. The comparison reached is the same
        // one either way: it asks whether the two disagree, not which moved.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedFile (io, kRealImage, RealDiskBytes());
        SeedFile (io, kHostFile,  MakePayload());

        io.mutateStampOnNextStat = true;

        result = runner.Run (MakePutOptions (kRealImage, "PROG"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("changed since it was read") != std::string::npos,
            L"the reason must be that the target moved");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertNamesOneReason      (result.diagnostics);

        Assert::AreEqual (0, io.writeCount, L"and nothing was written anywhere at all");

        AssertRefusalLeftEverythingAsItWas (io, kRealImage, RealDiskBytes(),
                                            kRealFileListed,
                                            { kRealImage, kHostFile });
    }

    TEST_METHOD (AnImageAnotherProgramHolds_RefusesThePutAndLeavesAMountableImageUnchanged)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedFile (io, kRealImage, RealDiskBytes());
        SeedFile (io, kHostFile,  MakePayload());

        io.reportHeldByOther = true;

        result = runner.Run (MakePutOptions (kRealImage, "PROG"));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);

        //  Against the runner's own constant rather than a retyped fragment.
        //  The help no longer carries a paragraph about the probe, so this
        //  refusal is the only place a user is told what happened -- and a
        //  reworded refusal that stopped saying what to do would otherwise
        //  still satisfy a substring of the old sentence.
        Assert::IsTrue (result.diagnostics.find (DiskCommandRunner::kInUseRefusalText)
                        != std::string::npos,
            L"the reason must name the other holder, and what to do about it");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertNamesOneReason      (result.diagnostics);

        Assert::AreEqual (0, io.writeCount, L"and nothing was written anywhere at all");

        AssertRefusalLeftEverythingAsItWas (io, kRealImage, RealDiskBytes(),
                                            kRealFileListed,
                                            { kRealImage, kHostFile });
    }
};
