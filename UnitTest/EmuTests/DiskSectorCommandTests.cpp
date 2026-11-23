#include "Pch.h"

#include "Machines/Apple2/Common/NibbleFormat.h"
#include "Machines/Apple2/Common/SectorInterleave.h"
#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/DiskImage.h"
#include "Machines/Apple2/Common/WozLoader.h"
#include "Devices/Disk/Inspector/FieldLocator.h"
#include "InspectorTrackBuilder.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskSectorCommandTests
//
//  disk sectorread and sectorwrite: bytes at a track and a sector, with no
//  filesystem involved, in logical and physical numbering.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskSectorCommandTests)
{
public:

    ////////////////////////////////////////////////////////////////////////////
    //
    //  sectorwrite: bytes at a track and a sector, no filesystem involved.
    //
    //  THE SECTOR IS LOGICAL AND THE FILE OFFSET IS NOT. They differ by the DOS
    //  3.3 interleave, and an implementation that ignored it would read back
    //  perfectly through our own reader while being garbage on real hardware.
    //  So the assertions below are against the SKEWED offsets, computed from
    //  the layer that owns the table rather than restated here.
    //
    ////////////////////////////////////////////////////////////////////////////

    static constexpr CommandLineOptions::DiskOptions::Numbering  kPhysical =
        CommandLineOptions::DiskOptions::Numbering::Physical;

    //  The numbering is a parameter with no default, mirroring the command
    //  line: a sector command that has not stated its numbering is a refusal,
    //  and a test that could quietly inherit one would not be testing the
    //  choice.
    static CommandLineOptions MakeSectorWrite (const char                                 * image,
                                               const char                                 * file,
                                               int                                          track,
                                               int                                          sector,
                                               CommandLineOptions::DiskOptions::Numbering   numbering)
    {
        CommandLineOptions  options;

        options.subcommand        = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::SectorWrite;
        options.disk.commandWord  = "sectorwrite";
        options.disk.imagePath    = image;
        options.disk.hostFile     = file;
        options.disk.track        = track;
        options.disk.sector       = sector;
        options.disk.numbering    = numbering;

        return options;
    }

    //  LOGICAL SECTOR 1 IS AT FILE OFFSET 256, because a .dsk keeps its
    //  sectors in DOS logical order and the command's numbers are logical.
    //  The second assertion is the regression proof: this command used to
    //  route the number through the physical interleave, landing these bytes
    //  on logical sector 7 -- silently, since sectorread applied the same
    //  wrong map and read them back perfectly.
    TEST_METHOD (SectorWrite_PlacesBytesAtTheLogicalSectorsOwnOffset)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (NibbleFormat::kSectorByteSize, (Byte) 0xA5);
        vector<Byte>       written;
        size_t             logicalAt = (size_t) (3 * 16 + 1) * 256;
        size_t             skewedAt  = 0;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 3, 1, DiskRunnerFixtures::kLogical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        skewedAt = (size_t) ((3 * NibbleFormat::kSectorsPerTrack
                            + SectorInterleave::GetDosFileIndexForPhysicalSector (1))
                           * NibbleFormat::kSectorByteSize);

        Assert::AreEqual ((Byte) 0xA5, written[logicalAt],
                          L"logical sector 1 of track 3 holds the bytes, at its own offset");

        Assert::AreNotEqual (logicalAt, skewedAt,
                             L"the two offsets must differ for this sector, or the assertion "
                             L"below discriminates nothing");

        Assert::AreEqual ((Byte) 0x00, written[skewedAt],
                          L"and the offset the old interleave routing would have hit is "
                          L"untouched");
    }

    //  A payload longer than one track runs on into the next, because splitting
    //  the call per track would put the wrap arithmetic back in the caller.
    TEST_METHOD (SectorWrite_RunsOnPastTheEndOfATrack)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (NibbleFormat::kSectorByteSize * 20, (Byte) 0x5A);
        vector<Byte>       written;
        size_t             lastAt = 0;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["big.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "big.bin", 1, 0, DiskRunnerFixtures::kLogical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        //  Twenty sectors from track 1 sector 0 run to index 19, which is
        //  four sectors into track 2: logical sector 3, at its own offset.
        lastAt = (size_t) ((2 * NibbleFormat::kSectorsPerTrack + 3)
                         * NibbleFormat::kSectorByteSize);

        Assert::AreEqual ((Byte) 0x5A, written[lastAt], L"the last sector landed on track 2");

        Assert::AreEqual ((Byte) 0x00,
                          written[(size_t) ((2 * NibbleFormat::kSectorsPerTrack + 4)
                                          * NibbleFormat::kSectorByteSize)],
                          L"and the one after it was left alone");
    }

    //  IT WRITES WHOLE SECTORS AND DISTURBS NOTHING ELSE. A payload that does
    //  not fill its last sector leaves the rest of that sector as it was.
    TEST_METHOD (SectorWrite_TouchesOnlyTheSectorsItWasGiven)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       written;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["short.bin"] = vector<Byte> (4, (Byte) 0xFF);

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "short.bin", 0, 0, DiskRunnerFixtures::kLogical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        Assert::AreEqual ((size_t) NibbleFormat::kImageByteSize, written.size(),
                          L"the image is still a whole disk");
        Assert::AreEqual ((Byte) 0xFF, written[0]);
        Assert::AreEqual ((Byte) 0x00, written[4],  L"the rest of the sector is untouched");
        Assert::AreEqual ((Byte) 0x00, written[256], L"and so is the next one");
    }

    //  A track or sector off the end of the disk is refused, not clamped.
    TEST_METHOD (SectorWrite_RefusesAPlaceThatIsNotOnTheDisk)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = vector<Byte> (16, (Byte) 0xA5);

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 35, 0, DiskRunnerFixtures::kLogical)).exitStatus,
                          L"track 35 is one past the last");
        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 0, 16, DiskRunnerFixtures::kLogical)).exitStatus,
                          L"and sector 16 is one past the last");
    }

    //  A payload that will not fit from where it was told to start is refused
    //  before anything is written, rather than truncated.
    TEST_METHOD (SectorWrite_RefusesAPayloadThatRunsOffTheEnd)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       before;
        vector<Byte>       after;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["big.bin"] = vector<Byte> ((size_t) NibbleFormat::kSectorByteSize * 40, (Byte) 0x5A);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", before));

        result = runner.Run (MakeSectorWrite ("raw.dsk", "big.bin", 33, 0, DiskRunnerFixtures::kLogical));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", after));
        Assert::IsTrue (before == after, L"and the image is exactly as it was");
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  sectorread: the same places, read back.
    //
    //  THE ROUND TRIP IS THE POINT. Before this existed, a disk written by
    //  sectorwrite could not be read by anything in the tool: get goes through
    //  a catalog and these disks have none. So the assertions that matter are
    //  the ones that put bytes in with one command and take them out with the
    //  other, through the same interleave, and compare.
    //
    ////////////////////////////////////////////////////////////////////////////

    //  WHAT ONE COMMAND WROTE, THE OTHER READS. Written at a logical sector and
    //  read back from the same one: if either applied the interleave and the
    //  other did not, the bytes would come back from somewhere else entirely.
    TEST_METHOD (SectorRead_ReturnsWhatSectorWritePutThere)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (NibbleFormat::kSectorByteSize, (Byte) 0xC3);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 3, 1, DiskRunnerFixtures::kLogical)).exitStatus);

        result = runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 3, 1, 1, DiskRunnerFixtures::kLogical));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload, L"with no --out the bytes are the payload");
        Assert::IsTrue (result.payload == payload,
                        L"and they are the bytes that were written, from the same logical sector");
    }

    //  A WOZ IN THE STANDARD LAYOUT, WITH ITS OWN VOLUME AND ITS RECORDS OUT OF
    //  TRACK ORDER, takes a sector write and gives the bytes back (GH #170).
    //  Writes used to refuse the layout, rebuild the track with volume 254, and
    //  take record N as track N; here track 13 lives in record 21.
    TEST_METHOD (SectorWrite_OnAStandardLayoutWozKeepsItsVolumeAndReadsBack)
    {
        FakeDiskFileIo        io;
        DiskCommandRunner     runner (io);
        vector<Byte>          woz;
        vector<Byte>          written;
        vector<Byte>          payload (NibbleFormat::kSectorByteSize, (Byte) 0x3C);
        DiskCommandResult     result;
        DiskImage             image;
        FramedTrack           track;
        vector<LocatedField>  fields;
        int                   addresses = 0;



        AssertSucceeded (InspectorTrackBuilder::MakeStandardWoz (130, false, true, woz));

        io.files["std.woz"]  = woz;
        io.stamps["std.woz"] = FileStamp { woz.size(), 1 };
        io.files["one.bin"]  = payload;

        result = runner.Run (MakeSectorWrite ("std.woz", "one.bin", 13, 3, DiskRunnerFixtures::kLogical));
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        result = runner.Run (DiskRunnerFixtures::MakeSectorRead ("std.woz", 13, 3, 1, DiskRunnerFixtures::kLogical));
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.payload == payload, L"the sector reads back from the record that holds track 13");

        AssertSucceeded (io.ReadAllBytes ("std.woz", written));
        AssertSucceeded (WozLoader::Load (written, image));
        Assert::AreEqual (21, image.ResolveWholeTrack (13));

        LatchFramer::Frame   (*TrackCopy::MakeFromImage (image, 21), track);
        FieldLocator::Locate (track, FieldMarks::MakeStandard(), fields);

        for (const LocatedField & f : fields)
        {
            if (f.role == FieldRole::Address)
            {
                Assert::AreEqual (130, static_cast<int> (f.volume), L"every address field keeps the disk's volume");
                addresses++;
            }
        }

        Assert::AreEqual (16, addresses);
    }

    //  A COUNT IS WHAT A READ HAS INSTEAD OF A LENGTH, and it spans tracks the
    //  same way a write does.
    TEST_METHOD (SectorRead_RunsOnPastTheEndOfATrack)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload ((size_t) NibbleFormat::kSectorByteSize * 3, (Byte) 0x7E);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["three.bin"] = payload;

        //  Sector 14 of track 1, so the third sector lands on track 2.
        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "three.bin", 1, 14, DiskRunnerFixtures::kLogical)).exitStatus);

        result = runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 1, 14, 3, DiskRunnerFixtures::kLogical));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.payload == payload,
                        L"all three sectors come back, across the track boundary");
    }

    //  WHOLE SECTORS, AND THE TAIL IS WHAT WAS ALREADY THERE. A read has no
    //  length to trim to, so a 10-byte write reads back as a full sector. That
    //  is not a defect to hide: it is what a disk with no catalog can tell you.
    TEST_METHOD (SectorRead_DeliversWholeSectors_BecauseNothingRecordsWhereBytesEnd)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (10, (Byte) 0x11);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["short.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "short.bin", 0, 0, DiskRunnerFixtures::kLogical)).exitStatus);

        result = runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 0, 0, 1, DiskRunnerFixtures::kLogical));

        Assert::AreEqual ((size_t) NibbleFormat::kSectorByteSize, result.payload.size(),
                          L"a whole sector, not the ten bytes that were written");

        Assert::IsTrue (std::equal (payload.begin(), payload.end(), result.payload.begin()),
                        L"and it opens with them");
    }

    //  --out WRITES THROUGH THE SEAM rather than to the payload, the same
    //  choice get makes, so a caller redirecting to a file and a caller
    //  piping get the same bytes.
    TEST_METHOD (SectorRead_WithAnOutFile_WritesThroughTheSeamInsteadOfThePayload)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        vector<Byte>        payload (NibbleFormat::kSectorByteSize, (Byte) 0x2D);
        CommandLineOptions  options = DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 5, 0, 1, DiskRunnerFixtures::kLogical);
        DiskCommandResult   result;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 5, 0, DiskRunnerFixtures::kLogical)).exitStatus);

        options.disk.hostFile = "back.bin";
        result                = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"an output file was given, so nothing goes to standard output");
        Assert::IsTrue (io.files["back.bin"] == payload, L"and the file holds the sector");
    }

    //  A FILESYSTEM IS NOT REQUIRED AND NOT LOOKED FOR, which is the whole
    //  reason the command exists: get refuses these disks outright.
    TEST_METHOD (SectorRead_ReadsADiskWithNoFilesystemAtAll)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");

        result = runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 0, 0, 1, DiskRunnerFixtures::kLogical));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
                          L"an unformatted disk is the ordinary case here, not a refusal");
    }

    TEST_METHOD (SectorRead_RefusesAPlaceThatIsNotOnTheDisk)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 35, 0, 1, DiskRunnerFixtures::kLogical)).exitStatus,
                          L"there is no track 35");

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 0, 16, 1, DiskRunnerFixtures::kLogical)).exitStatus,
                          L"and no sector 16");
    }

    //  ZERO SECTORS IS NOT A READ, and neither is a negative one. Left
    //  unchecked the first would deliver nothing and report success.
    TEST_METHOD (SectorRead_RefusesACountThatIsNotAReadAtAll)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 0, 0, 0, DiskRunnerFixtures::kLogical)).exitStatus,
                          L"zero sectors");

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 0, 0, -1, DiskRunnerFixtures::kLogical)).exitStatus,
                          L"and fewer than that");
    }

    TEST_METHOD (SectorRead_RefusesACountThatRunsOffTheEnd)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");

        result = runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 34, 14, 5, DiskRunnerFixtures::kLogical));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine);
        Assert::IsFalse (result.hasPayload, L"and no partial read is handed back");
    }

    //  THE NUMBERING HAS NO DEFAULT, and this is the refusal that enforces
    //  it. A sector command that guessed is exactly how bytes once landed on
    //  the wrong sector -- silently, because both commands guessed the same
    //  way -- so an unstated numbering is an instructive refusal, not a
    //  fallback.
    TEST_METHOD (Sector_WithoutSayingWhichNumbering_IsRefusedInstructively)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  read  = DiskRunnerFixtures::MakeSectorRead  ("raw.dsk", 0, 0, 1, DiskRunnerFixtures::kLogical);
        CommandLineOptions  write = MakeSectorWrite ("raw.dsk", "one.bin", 0, 0, DiskRunnerFixtures::kLogical);
        DiskCommandResult   result;
        vector<Byte>        before;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = DiskRunnerFixtures::MakePayload (256);

        before          = io.files["raw.dsk"];
        read.disk.numbering  = CommandLineOptions::DiskOptions::Numbering::Unstated;
        write.disk.numbering = CommandLineOptions::DiskOptions::Numbering::Unstated;

        result = runner.Run (read);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a read with no numbering is refused");
        Assert::IsTrue (result.diagnostics.find ("--logical or --physical") != std::string::npos,
            L"and the refusal shows what to type, not merely that something is missing");

        result = runner.Run (write);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a write with no numbering is refused");
        Assert::IsTrue (result.diagnostics.find ("--logical or --physical") != std::string::npos);

        Assert::IsTrue (io.files["raw.dsk"] == before,
            L"and the image is byte for byte what it was");
    }

    //  THE PHYSICAL LENS, against the same offsets the logical test pins the
    //  other way around: physical sector 1 is the address mark the drive
    //  presents second, and a DOS-ordered image keeps the sector under it at
    //  the offset the interleave picks -- logical 7.
    TEST_METHOD (SectorWrite_Physical_PlacesBytesUnderTheAddressMark)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (NibbleFormat::kSectorByteSize, (Byte) 0xC3);
        vector<Byte>       written;
        size_t             mappedAt  = 0;
        size_t             literalAt = (size_t) (3 * 16 + 1) * 256;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 3, 1, kPhysical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        mappedAt = (size_t) ((3 * NibbleFormat::kSectorsPerTrack
                            + SectorInterleave::GetDosFileIndexForPhysicalSector (1))
                           * NibbleFormat::kSectorByteSize);

        Assert::AreNotEqual (literalAt, mappedAt,
                             L"the two offsets must differ for this sector, or nothing below "
                             L"discriminates the lenses");

        Assert::AreEqual ((Byte) 0xC3, written[mappedAt],
                          L"physical sector 1 lands where the interleave puts it");

        Assert::AreEqual ((Byte) 0x00, written[literalAt],
                          L"and not at the logical offset of the same number");
    }

    //  THE LENSES ARE ONE MAPPING, READ FROM BOTH SIDES: what a physical
    //  write lays down, a physical read returns from the same numbers, and a
    //  logical read finds at the interleave's logical sector.
    TEST_METHOD (SectorRead_Physical_AgreesWithTheLogicalLensAboutTheSameBytes)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload = DiskRunnerFixtures::MakePayload (256);
        DiskCommandResult  asPhysical;
        DiskCommandResult  asLogical;

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 3, 1, kPhysical)).exitStatus);

        asPhysical = runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 3, 1, 1, kPhysical));
        asLogical  = runner.Run (DiskRunnerFixtures::MakeSectorRead ("raw.dsk", 3,
                                                 SectorInterleave::GetDosFileIndexForPhysicalSector (1),
                                                 1, DiskRunnerFixtures::kLogical));

        Assert::AreEqual (DiskCommandResult::kClean, asPhysical.exitStatus);
        Assert::AreEqual (DiskCommandResult::kClean, asLogical.exitStatus);

        Assert::IsTrue (asPhysical.payload == payload,
            L"the physical read returns what the physical write put at the same numbers");

        Assert::IsTrue (asLogical.payload == payload,
            L"and the logical read finds the same bytes at the interleave's logical sector");
    }

    //  A MULTI-SECTOR PHYSICAL WRITE ADVANCES BY ADDRESS MARK, so page N of
    //  the payload sits under mark N -- what a boot loader that files
    //  sectors by address mark reads back in order. This is the layout the
    //  demo disk needs, expressed as the command rather than as a
    //  hand-permuted payload.
    TEST_METHOD (SectorWrite_Physical_LaysARunUnderConsecutiveAddressMarks)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (NibbleFormat::kSectorByteSize * 16, 0);
        vector<Byte>       written;
        size_t             page = 0;

        //  Every byte of a page carries the page number, so a page in the
        //  wrong place is visible wherever it is looked at.
        for (page = 0; page < 16; page++)
        {
            std::fill_n (payload.begin() + (ptrdiff_t) (page * 256), 256, (Byte) page);
        }

        DiskRunnerFixtures::SeedBlankImage (io, "raw.dsk");
        io.files["track.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "track.bin", 3, 0, kPhysical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        for (page = 0; page < 16; page++)
        {
            size_t  at = (size_t) ((3 * NibbleFormat::kSectorsPerTrack
                                  + SectorInterleave::GetDosFileIndexForPhysicalSector ((int) page))
                                 * NibbleFormat::kSectorByteSize);

            Assert::AreEqual ((int) page, (int) written[at],
                L"page N sits under address mark N, wherever the interleave keeps that");
        }
    }
};
