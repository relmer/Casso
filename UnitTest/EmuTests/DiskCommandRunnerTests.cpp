#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/DirectBootBuilder.h"
#include "Devices/Disk/NibblizationLayer.h"
#include "Devices/Disk/Dos33Skeleton.h"
#include "Devices/Disk/Dos33Volume.h"
#include "Devices/Disk/ProDosSkeleton.h"
#include "Devices/Disk/VolumeImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommandRunnerTests
//
//  The whole command path, from parsed options to what the executable would
//  print, with a real 1984 disk seeded into an in-memory platform.
//
//  Nothing here touches a file. The runner reaches the host only through the
//  seam, which is what makes the path testable at all -- the test assembly does
//  not link the console executable, so any decision placed there would be
//  unreachable from here.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskCommandRunnerTests)
{
public:

    static constexpr const char *  kImage = "C:\\disks\\merlin.dsk";

    //  Seeds the fake platform with a real disk, so the command path is
    //  exercised against material nobody here authored.
    void SeedRealDisk (FakeDiskFileIo & io, const char * fixture = "Disks/Merlin-proDos2.23.dsk",
                       const char * asPath = kImage)
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture (fixture, bytes));

        io.files[asPath]  = bytes;
        io.stamps[asPath] = FileStamp { bytes.size(), 100 };
    }

    CommandLineOptions MakeOptions (CommandLineOptions::DiskOptions::Command command,
                                    const char * image = kImage)
    {
        CommandLineOptions  options;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = command;
        options.disk.imagePath = image;

        return options;
    }

    //
    //  A LISTING HAS TO FIT AN 80-COLUMN TERMINAL, and that is the constraint
    //  that made the ProDOS columns safe to print unconditionally rather than
    //  behind a flag. A row that wraps is a row that reads as two entries.
    //
    static void AssertEveryLineFitsEightyColumns (const std::string & text)
    {
        size_t  lineStart = 0;
        size_t  lineEnd   = 0;

        while (lineStart < text.size())
        {
            std::string  line;

            lineEnd = text.find ('\n', lineStart);
            line    = (lineEnd == std::string::npos)
                          ? text.substr (lineStart)
                          : text.substr (lineStart, lineEnd - lineStart);

            Assert::IsTrue (line.size() <= 80,
                (std::wstring (L"line runs past column 80: ")
                     + std::wstring (line.begin(), line.end())).c_str());

            if (lineEnd == std::string::npos)
            {
                break;
            }

            lineStart = lineEnd + 1;
        }
    }

    TEST_METHOD (List_RealDisk_RendersEveryCatalogEntryIncludingHeadings)
    {
        // The vendor's own printed catalog shows all sixty-three rows, twenty
        // of which occupy no sectors and exist to draw section headings. This
        // listing renders them for the same reason DOS does: they are what
        // their author put on the disk to be seen, and hiding them would make
        // this output disagree with the machine's.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        size_t             lines = 0;
        size_t             at    = 0;

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"a disk that shipped must list cleanly");

        Assert::IsTrue (result.output.find ("DISK VOLUME 254") != std::string::npos,
            L"the volume number is part of the listing");

        Assert::IsTrue (result.output.find ("MERLIN PRO - DOS 3.3") != std::string::npos,
            L"heading rows are rendered, not filtered");

        Assert::IsTrue (result.output.find ("*T 000 ") != std::string::npos,
            L"and they render in the shape the guest prints: locked, type T, zero sectors");

        Assert::IsTrue (result.output.find (" A 004 HELLO") != std::string::npos,
            L"alongside real files in the same format");

        while ((at = result.output.find ('\n', at)) != std::string::npos)
        {
            lines++;
            at++;
        }

        // 63 entries, a volume header line, a blank, a blank and a free-space
        // line. The exact total matters less than that no entry was dropped.
        Assert::IsTrue (lines >= 63, L"every catalog entry must appear");
    }

    TEST_METHOD (List_ReportsFreeSpace)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::IsTrue (result.output.find ("sectors free of 560") != std::string::npos,
            L"the listing states free space against the volume's capacity");
    }

    TEST_METHOD (List_ProDosVolume_UsesItsOwnShape)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", "C:\\disks\\merlin.po.dsk");

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                          "C:\\disks\\merlin.po.dsk"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("/MERLIN") != std::string::npos,
            L"a ProDOS volume is named, not numbered");
        Assert::IsTrue (result.output.find ("blocks free of 280") != std::string::npos,
            L"and counts in blocks rather than sectors");
    }

    //  The independently extracted copy, minus the four-byte DOS 3.3 header the
    //  reader consumes. Comparing against these bytes is strictly stronger than
    //  comparing a length: a length check passes under any corruption that
    //  happens to preserve size, and the oracle is already sitting there.
    vector<Byte> ExpectedMakeDumpPayload()
    {
        FixtureProvider  fixtures;
        vector<Byte>     stored;

        AssertSucceeded (fixtures.OpenFixture ("Merlin/MAKE DUMP", stored));
        Assert::AreEqual (size_t (593), stored.size(), L"the stored file carries its header");

        return vector<Byte> (stored.begin() + 4, stored.end());
    }

    void AssertIsMakeDumpPayload (const vector<Byte> & actual)
    {
        vector<Byte>  expected = ExpectedMakeDumpPayload();
        size_t        i        = 0;

        // 589 is the length the header declares. Naming it here makes the
        // failure legible: 618 would mean 29 line-feed bytes were expanded on
        // the way out, which is the text-mode corruption this file's payload is
        // uniquely suited to expose.
        Assert::AreEqual (size_t (589), expected.size());
        Assert::AreEqual (expected.size(), actual.size());

        for (i = 0; i < expected.size(); i++)
        {
            if (actual[i] != expected[i])
            {
                Assert::Fail (L"payload differs from the independently extracted copy");
            }
        }
    }

    TEST_METHOD (Get_WithNoOutputFile_ReturnsPayloadForStandardOutput)
    {
        // With no destination named the bytes go to the process's own output,
        // which is what makes extraction pipeable.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload, L"the payload is returned for the caller to deliver");
        AssertIsMakeDumpPayload (result.payload);
        Assert::AreEqual (size_t (0), io.files.count ("C:\\out.bin"),
            L"and nothing was written to the host");
    }

    TEST_METHOD (Get_WithOutputFile_WritesThroughTheSeam)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path     = "MAKE DUMP";
        options.disk.hostFile = "C:\\out.bin";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"a named destination is not the process output");
        Assert::AreEqual (size_t (1), io.files.count ("C:\\out.bin"));
        AssertIsMakeDumpPayload (io.files["C:\\out.bin"]);
    }

    TEST_METHOD (Get_PayloadCorrectnessStopsAtTheSeam_NotAtTheHostEdge)
    {
        // WHAT THIS TEST DOES NOT PROVE, stated because the omission is easy to
        // miss and a green run here looks exactly like coverage.
        //
        // MAKE DUMP's payload contains 29 line-feed bytes and no pre-existing
        // CR/LF pair. Delivered through a standard output left in text mode,
        // those 29 become 58 and the file arrives as 618 bytes rather than 589
        // -- purely additive corruption, so any count other than those two
        // numbers means something else is wrong.
        //
        // That translation happens in the runtime BELOW the seam, at the host
        // edge. The substitute used here does not translate, so this assertion
        // passes whether or not the edge sets binary mode. The fake's own
        // header says as much; this test says it at the point of temptation.
        //
        // No automated test can cover it: unit tests may not touch real system
        // state, and no test may run the console binary. The edge is therefore
        // verified by the manual check in quickstart.md, and the mode call
        // carries a comment saying why it must not be removed.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions  options   = MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult   result;
        int                 lineFeeds = 0;

        SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        for (Byte b : result.payload)
        {
            if (b == 0x0A) { lineFeeds++; }
        }

        Assert::AreEqual (29, lineFeeds,
            L"the payload carries the line feeds that make the host edge's mode matter");
    }

    TEST_METHOD (Get_ReportsTheLoadAddressOnTheErrorStream)
    {
        // The load address is information about the extraction, not part of it.
        // Putting it on the diagnostic stream keeps piped output byte-exact.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        Assert::IsTrue (result.diagnostics.find ("$9000") != std::string::npos,
            L"the load address must be reported");
        Assert::IsTrue (result.output.empty(),
            L"and must not contaminate the payload stream");
    }

    TEST_METHOD (Get_MissingFile_ProducesNoOutputAndNamesBoth)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path = "NOSUCHFILE";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (result.hasPayload);
        Assert::IsTrue (result.diagnostics.find (kImage) != std::string::npos,
            L"the message names the image");
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos,
            L"and the file");
    }

    TEST_METHOD (UnreadableImage_ProducesNoOutput)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.output.empty());
        Assert::IsTrue (result.diagnostics.find ("cannot be read") != std::string::npos);
    }

    TEST_METHOD (UnrecognizedFilesystem_IsRefusedRatherThanGuessed)
    {
        // Refusing to name a filesystem is an answer. Guessing one would send
        // the reader at structures that are not there and report whatever it
        // found as a catalog.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       noise (NibblizationLayer::kImageByteSize, 0xE5);

        io.files[kImage]  = noise;
        io.stamps[kImage] = FileStamp { noise.size(), 1 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.output.empty(), L"and nothing is offered as a catalog");
        Assert::IsTrue (result.diagnostics.find (DiskImageSession::kNoFilesystemText)
                            != std::string::npos,
            L"in the words a person would use");
    }


    ////////////////////////////////////////////////////////////////////////////
    //
    //  The console shares the emulator's reasons
    //
    //  Both arrive at a refusal through the same loaders, so a reason only one
    //  of them could give would be a reason the other went looking for and did
    //  not find. These assert on the shared clause, which is what makes the
    //  sharing real rather than coincidental.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (TruncatedImage_IsRefusedWithItsLength)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       truncated (4096, 0);

        io.files[kImage]  = truncated;
        io.stamps[kImage] = FileStamp { truncated.size(), 1 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("4,096 bytes") != std::string::npos,
            L"the refusal says how big the file is");
        Assert::IsTrue (result.diagnostics.find ("143,360 bytes") != std::string::npos,
            L"and how big a .dsk has to be, which is what identifies a bad download");
    }


    TEST_METHOD (RenamedWozImage_IsRefusedAsNotAWoz)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       renamed (600, 0x41);
        const char *       wozPath = "C:\\disks\\notreally.woz";

        io.files[wozPath]  = renamed;
        io.stamps[wozPath] = FileStamp { renamed.size(), 1 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                          wozPath));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("WOZ file header") != std::string::npos,
            L"a .woz with no WOZ header is told exactly that, not 'not a disk image'");
    }


    TEST_METHOD (EmptyImageFile_IsRefusedAsEmpty)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        vector<Byte>       nothing;

        io.files[kImage]  = nothing;
        io.stamps[kImage] = FileStamp { 0, 1 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("is empty") != std::string::npos,
            L"a zero-byte file is empty, and saying so beats an arithmetic complaint");
    }

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

    //  Short names for the numbering a maker is called with, so a call site
    //  reads as the command line would.
    static constexpr CommandLineOptions::DiskOptions::Numbering  kLogical =
        CommandLineOptions::DiskOptions::Numbering::Logical;

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

    //  A blank .dsk to write into, with no filesystem on it at all.
    //
    //  THE STAMP MATTERS AS MUCH AS THE BYTES. A commit compares the file it
    //  is about to replace against the one that was read, and refuses when it
    //  cannot: an image seeded with contents and no stamp is one the runner
    //  will decline to write, which is correct of it and easy to mistake for
    //  a bug in the command under test.
    static void SeedBlankImage (FakeDiskFileIo & io, const char * path)
    {
        FileStamp  stamp;

        io.files[path] = vector<Byte> ((size_t) NibblizationLayer::kImageByteSize, (Byte) 0x00);

        stamp.sizeBytes   = (uint64_t) io.files[path].size();
        stamp.modifiedUnix = 1;

        io.stamps[path] = stamp;
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
        vector<Byte>       payload (NibblizationLayer::kSectorByteSize, (Byte) 0xA5);
        vector<Byte>       written;
        size_t             logicalAt = (size_t) (3 * 16 + 1) * 256;
        size_t             skewedAt  = 0;

        SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 3, 1, kLogical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        skewedAt = (size_t) ((3 * NibblizationLayer::kSectorsPerTrack
                            + NibblizationLayer::GetDosFileIndexForPhysicalSector (1))
                           * NibblizationLayer::kSectorByteSize);

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
        vector<Byte>       payload (NibblizationLayer::kSectorByteSize * 20, (Byte) 0x5A);
        vector<Byte>       written;
        size_t             lastAt = 0;

        SeedBlankImage (io, "raw.dsk");
        io.files["big.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "big.bin", 1, 0, kLogical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        //  Twenty sectors from track 1 sector 0 run to index 19, which is
        //  four sectors into track 2: logical sector 3, at its own offset.
        lastAt = (size_t) ((2 * NibblizationLayer::kSectorsPerTrack + 3)
                         * NibblizationLayer::kSectorByteSize);

        Assert::AreEqual ((Byte) 0x5A, written[lastAt], L"the last sector landed on track 2");

        Assert::AreEqual ((Byte) 0x00,
                          written[(size_t) ((2 * NibblizationLayer::kSectorsPerTrack + 4)
                                          * NibblizationLayer::kSectorByteSize)],
                          L"and the one after it was left alone");
    }

    //  IT WRITES WHOLE SECTORS AND DISTURBS NOTHING ELSE. A payload that does
    //  not fill its last sector leaves the rest of that sector as it was.
    TEST_METHOD (SectorWrite_TouchesOnlyTheSectorsItWasGiven)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       written;

        SeedBlankImage (io, "raw.dsk");
        io.files["short.bin"] = vector<Byte> (4, (Byte) 0xFF);

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "short.bin", 0, 0, kLogical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        Assert::AreEqual ((size_t) NibblizationLayer::kImageByteSize, written.size(),
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

        SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = vector<Byte> (16, (Byte) 0xA5);

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 35, 0, kLogical)).exitStatus,
                          L"track 35 is one past the last");
        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 0, 16, kLogical)).exitStatus,
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

        SeedBlankImage (io, "raw.dsk");
        io.files["big.bin"] = vector<Byte> ((size_t) NibblizationLayer::kSectorByteSize * 40, (Byte) 0x5A);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", before));

        result = runner.Run (MakeSectorWrite ("raw.dsk", "big.bin", 33, 0, kLogical));

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

    static CommandLineOptions MakeSectorRead (const char                                 * image,
                                              int                                          track,
                                              int                                          sector,
                                              int                                          count,
                                              CommandLineOptions::DiskOptions::Numbering   numbering)
    {
        CommandLineOptions  options;

        options.subcommand        = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::SectorRead;
        options.disk.commandWord  = "sectorread";
        options.disk.imagePath    = image;
        options.disk.track        = track;
        options.disk.sector       = sector;
        options.disk.count        = count;
        options.disk.numbering    = numbering;

        return options;
    }

    static CommandLineOptions MakeBlockRead (const char * image, int block, int count)
    {
        CommandLineOptions  options;

        options.subcommand        = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::BlockRead;
        options.disk.commandWord  = "blockread";
        options.disk.imagePath    = image;
        options.disk.block        = block;
        options.disk.count        = count;

        return options;
    }

    static CommandLineOptions MakeBlockWrite (const char * image, const char * file, int block)
    {
        CommandLineOptions  options;

        options.subcommand        = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::BlockWrite;
        options.disk.commandWord  = "blockwrite";
        options.disk.imagePath    = image;
        options.disk.hostFile     = file;
        options.disk.block        = block;

        return options;
    }

    //  WHAT ONE COMMAND WROTE, THE OTHER READS. Written at a logical sector and
    //  read back from the same one: if either applied the interleave and the
    //  other did not, the bytes would come back from somewhere else entirely.
    TEST_METHOD (SectorRead_ReturnsWhatSectorWritePutThere)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (NibblizationLayer::kSectorByteSize, (Byte) 0xC3);
        DiskCommandResult  result;

        SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 3, 1, kLogical)).exitStatus);

        result = runner.Run (MakeSectorRead ("raw.dsk", 3, 1, 1, kLogical));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload, L"with no --out the bytes are the payload");
        Assert::IsTrue (result.payload == payload,
                        L"and they are the bytes that were written, from the same logical sector");
    }

    //  A COUNT IS WHAT A READ HAS INSTEAD OF A LENGTH, and it spans tracks the
    //  same way a write does.
    TEST_METHOD (SectorRead_RunsOnPastTheEndOfATrack)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload ((size_t) NibblizationLayer::kSectorByteSize * 3, (Byte) 0x7E);
        DiskCommandResult  result;

        SeedBlankImage (io, "raw.dsk");
        io.files["three.bin"] = payload;

        //  Sector 14 of track 1, so the third sector lands on track 2.
        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "three.bin", 1, 14, kLogical)).exitStatus);

        result = runner.Run (MakeSectorRead ("raw.dsk", 1, 14, 3, kLogical));

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

        SeedBlankImage (io, "raw.dsk");
        io.files["short.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "short.bin", 0, 0, kLogical)).exitStatus);

        result = runner.Run (MakeSectorRead ("raw.dsk", 0, 0, 1, kLogical));

        Assert::AreEqual ((size_t) NibblizationLayer::kSectorByteSize, result.payload.size(),
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
        vector<Byte>        payload (NibblizationLayer::kSectorByteSize, (Byte) 0x2D);
        CommandLineOptions  options = MakeSectorRead ("raw.dsk", 5, 0, 1, kLogical);
        DiskCommandResult   result;

        SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 5, 0, kLogical)).exitStatus);

        options.disk.hostFile = "back.bin";
        result                = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"named a file, so nothing goes to standard output");
        Assert::IsTrue (io.files["back.bin"] == payload, L"and the file holds the sector");
    }

    //  A FILESYSTEM IS NOT REQUIRED AND NOT LOOKED FOR, which is the whole
    //  reason the command exists: get refuses these disks outright.
    TEST_METHOD (SectorRead_ReadsADiskWithNoFilesystemAtAll)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedBlankImage (io, "raw.dsk");

        result = runner.Run (MakeSectorRead ("raw.dsk", 0, 0, 1, kLogical));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
                          L"an unformatted disk is the ordinary case here, not a refusal");
    }

    TEST_METHOD (SectorRead_RefusesAPlaceThatIsNotOnTheDisk)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);

        SeedBlankImage (io, "raw.dsk");

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (MakeSectorRead ("raw.dsk", 35, 0, 1, kLogical)).exitStatus,
                          L"there is no track 35");

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (MakeSectorRead ("raw.dsk", 0, 16, 1, kLogical)).exitStatus,
                          L"and no sector 16");
    }

    //  ZERO SECTORS IS NOT A READ, and neither is a negative one. Left
    //  unchecked the first would deliver nothing and report success.
    TEST_METHOD (SectorRead_RefusesACountThatIsNotAReadAtAll)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);

        SeedBlankImage (io, "raw.dsk");

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (MakeSectorRead ("raw.dsk", 0, 0, 0, kLogical)).exitStatus,
                          L"zero sectors");

        Assert::AreEqual (DiskCommandResult::kNoOutput,
                          runner.Run (MakeSectorRead ("raw.dsk", 0, 0, -1, kLogical)).exitStatus,
                          L"and fewer than that");
    }

    TEST_METHOD (SectorRead_RefusesACountThatRunsOffTheEnd)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedBlankImage (io, "raw.dsk");

        result = runner.Run (MakeSectorRead ("raw.dsk", 34, 14, 5, kLogical));

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
        CommandLineOptions  read  = MakeSectorRead  ("raw.dsk", 0, 0, 1, kLogical);
        CommandLineOptions  write = MakeSectorWrite ("raw.dsk", "one.bin", 0, 0, kLogical);
        DiskCommandResult   result;
        vector<Byte>        before;

        SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = MakePayload (256);

        before          = io.files["raw.dsk"];
        read.disk.numbering  = CommandLineOptions::DiskOptions::Numbering::Unstated;
        write.disk.numbering = CommandLineOptions::DiskOptions::Numbering::Unstated;

        result = runner.Run (read);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a read with no numbering is refused");
        Assert::IsTrue (result.diagnostics.find ("--logical or --physical") != std::string::npos,
            L"and the refusal says what to type, not merely that something is missing");

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
        vector<Byte>       payload (NibblizationLayer::kSectorByteSize, (Byte) 0xC3);
        vector<Byte>       written;
        size_t             mappedAt  = 0;
        size_t             literalAt = (size_t) (3 * 16 + 1) * 256;

        SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 3, 1, kPhysical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        mappedAt = (size_t) ((3 * NibblizationLayer::kSectorsPerTrack
                            + NibblizationLayer::GetDosFileIndexForPhysicalSector (1))
                           * NibblizationLayer::kSectorByteSize);

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
        vector<Byte>       payload = MakePayload (256);
        DiskCommandResult  asPhysical;
        DiskCommandResult  asLogical;

        SeedBlankImage (io, "raw.dsk");
        io.files["one.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "one.bin", 3, 1, kPhysical)).exitStatus);

        asPhysical = runner.Run (MakeSectorRead ("raw.dsk", 3, 1, 1, kPhysical));
        asLogical  = runner.Run (MakeSectorRead ("raw.dsk", 3,
                                                 NibblizationLayer::GetDosFileIndexForPhysicalSector (1),
                                                 1, kLogical));

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
        vector<Byte>       payload (NibblizationLayer::kSectorByteSize * 16, 0);
        vector<Byte>       written;
        size_t             page = 0;

        //  Every byte of a page carries the page number, so a page in the
        //  wrong place is visible wherever it is looked at.
        for (page = 0; page < 16; page++)
        {
            std::fill_n (payload.begin() + (ptrdiff_t) (page * 256), 256, (Byte) page);
        }

        SeedBlankImage (io, "raw.dsk");
        io.files["track.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeSectorWrite ("raw.dsk", "track.bin", 3, 0, kPhysical)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.dsk", written));

        for (page = 0; page < 16; page++)
        {
            size_t  at = (size_t) ((3 * NibblizationLayer::kSectorsPerTrack
                                  + NibblizationLayer::GetDosFileIndexForPhysicalSector ((int) page))
                                 * NibblizationLayer::kSectorByteSize);

            Assert::AreEqual ((int) page, (int) written[at],
                L"page N sits under address mark N, wherever the interleave keeps that");
        }
    }




    ////////////////////////////////////////////////////////////////////////////
    //
    //  blockread / blockwrite: the 512-byte ProDOS view, through the same
    //  single block map the ProDOS reader and writer use.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (BlockWrite_PlacesBothHalvesWhereTheProDosMapPutsThem)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>  payload  = MakePayload (512);
        vector<Byte>  written;
        size_t        firstAt  = ProDosSkeleton::BlockByteOffset (3, 0);
        size_t        secondAt = ProDosSkeleton::BlockByteOffset (3, 256);
        size_t        naiveAt  = (size_t) 3 * 512;
        size_t        i        = 0;

        SeedBlankImage (io, "raw.po.dsk");
        io.files["block.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeBlockWrite ("raw.po.dsk", "block.bin", 3)).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("raw.po.dsk", written));

        for (i = 0; i < 256; i++)
        {
            Assert::AreEqual ((int) payload[i],       (int) written[firstAt + i],
                L"the block's first half sits where the ProDOS map puts it");
            Assert::AreEqual ((int) payload[256 + i], (int) written[secondAt + i],
                L"and the second half where the map puts that");
        }

        Assert::AreNotEqual (naiveAt, firstAt,
            L"which for this block is not 512 * N into the buffer, or the assertions "
            L"above discriminate nothing");
    }

    TEST_METHOD (BlockRead_ReturnsWhatBlockWritePutThere)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload = MakePayload (1024);
        DiskCommandResult  result;

        SeedBlankImage (io, "raw.po.dsk");
        io.files["blocks.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeBlockWrite ("raw.po.dsk", "blocks.bin", 7)).exitStatus);

        result = runner.Run (MakeBlockRead ("raw.po.dsk", 7, 2));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload);
        Assert::IsTrue (result.payload == payload,
            L"two blocks in, the same two blocks out, through the one block map");
    }

    //  THE LENS WORKS ON ANY CONTAINER. A ProDOS volume shipped inside a
    //  .dsk reads by block number exactly as a .po would, because the
    //  session normalizes every image into the same buffer -- and the proof
    //  is vendor material: the Merlin disk's volume directory key block
    //  carries its own volume name.
    TEST_METHOD (BlockRead_FindsTheVolumeDirectory_OnAProDosVolumeInADskContainer)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;
        std::string        text;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kImage);

        result = runner.Run (MakeBlockRead (kImage, 2, 1));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload);
        Assert::AreEqual (size_t (512), result.payload.size(), L"one whole block");

        text.assign (result.payload.begin(), result.payload.end());

        Assert::IsTrue (text.find ("MERLIN") != std::string::npos,
            L"the volume directory key block names the volume, which only a correct "
            L"block map could have assembled from its two sector records");
    }

    TEST_METHOD (Block_OffTheDisk_IsRefusedWithTheRange)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedBlankImage (io, "raw.po.dsk");
        io.files["block.bin"] = MakePayload (512);

        result = runner.Run (MakeBlockRead ("raw.po.dsk", 280, 1));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"there is no block 280");
        Assert::IsTrue (result.diagnostics.find ("0-279") != std::string::npos,
            L"and the refusal states the range");

        result = runner.Run (MakeBlockWrite ("raw.po.dsk", "block.bin", 279));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"while the last block on the disk is a legal place for one block");

        result = runner.Run (MakeBlockRead ("raw.po.dsk", 279, 2));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"and a read that runs off the end is refused rather than shortened");
    }




    ////////////////////////////////////////////////////////////////////////////
    //
    //  create --boot: a disk that starts a binary with no operating system.
    //
    //  The builder underneath is already gated by a real-CPU test that boots a
    //  6502 over its output. What these cover is the half that did not exist
    //  until now: reaching it from a command, and writing what it produces into
    //  the container the caller asked for.
    //
    ////////////////////////////////////////////////////////////////////////////

    static CommandLineOptions MakeDirectBoot (const char * image, const char * payload)
    {
        CommandLineOptions  options = MakeCreate (image);

        options.disk.directBootFile = payload;

        return options;
    }

    //  THE COMMAND AND THE BUILDER AGREE, byte for byte. A .dsk is the sector
    //  buffer verbatim, so the whole path can be checked against the thing the
    //  guest-visible tests already boot, with no CPU in the loop.
    TEST_METHOD (DirectBoot_WritesExactlyWhatTheBuilderProduced)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (32, (Byte) 0xEA);
        vector<Byte>       expected;
        vector<Byte>       written;
        DirectBootSpec     spec;
        std::string        refusal;
        DiskCommandResult  result;

        io.files["prog.bin"] = payload;

        result = runner.Run (MakeDirectBoot ("boot.dsk", "prog.bin"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        AssertSucceeded (DirectBootBuilder::Build (payload, spec, expected, refusal));
        AssertSucceeded (io.ReadAllBytes ("boot.dsk", written));

        Assert::IsTrue (written == expected, L"the image is the builder's own sectors");
    }

    //  The container follows the name here as it does everywhere else, and a
    //  WOZ is a bit stream rather than sectors, so its size differs.
    TEST_METHOD (DirectBoot_HonorsTheContainerTheNameAsksFor)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        vector<Byte>       payload (32, (Byte) 0xEA);
        vector<Byte>       written;

        io.files["prog.bin"] = payload;

        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeDirectBoot ("boot.po", "prog.bin")).exitStatus);
        Assert::AreEqual (DiskCommandResult::kClean,
                          runner.Run (MakeDirectBoot ("boot.woz", "prog.bin")).exitStatus);

        AssertSucceeded (io.ReadAllBytes ("boot.po", written));
        Assert::AreEqual ((size_t) NibblizationLayer::kImageByteSize, written.size(),
                          L"a .po is the same sectors in ProDOS order");

        AssertSucceeded (io.ReadAllBytes ("boot.woz", written));
        Assert::IsTrue (written.size() > (size_t) NibblizationLayer::kImageByteSize,
                        L"and a .woz is a bit stream, which is larger");
    }

    //  AN ENTRY AWAY FROM THE LOAD ADDRESS REACHES THE BUILDER. A payload whose
    //  first byte is a header rather than an instruction is ordinary, and the
    //  flag exists so it does not have to be rebuilt to boot.
    TEST_METHOD (DirectBoot_PassesTheEntryAddressThrough)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        vector<Byte>        payload (64, (Byte) 0xEA);
        vector<Byte>        expected;
        vector<Byte>        written;
        CommandLineOptions  options = MakeDirectBoot ("boot.dsk", "prog.bin");
        DirectBootSpec      spec;
        std::string         refusal;

        io.files["prog.bin"] = payload;

        options.disk.entryAddress    = 0x0920;
        options.disk.hasEntryAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        spec.entryAddress = 0x0920;
        AssertSucceeded (DirectBootBuilder::Build (payload, spec, expected, refusal));
        AssertSucceeded (io.ReadAllBytes ("boot.dsk", written));

        Assert::IsTrue (written == expected, L"the entry the caller named is the one built in");
    }

    //  THE TWO WAYS TO BOOT ARE REFUSED TOGETHER. Measured before the check
    //  existed, the pair honored --boot and dropped --bootable silently,
    //  because this path never reaches the code that would have caught it.
    TEST_METHOD (DirectBoot_AndCopyingAnOperatingSystem_AreRefusedTogether)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeDirectBoot ("boot.dsk", "prog.bin");
        DiskCommandResult   result;

        io.files["prog.bin"] = vector<Byte> (32, (Byte) 0xEA);

        options.disk.bootable = true;
        result                = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine);
        Assert::IsFalse (io.Exists ("boot.dsk"), L"and nothing was written");
    }

    //  A direct-boot disk holds the binary and nothing else, so a filesystem
    //  asked for alongside it is refused rather than one of them dropped.
    TEST_METHOD (DirectBoot_WithAFilesystemAskedFor_IsRefused)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeDirectBoot ("boot.dsk", "prog.bin");
        DiskCommandResult   result;

        io.files["prog.bin"] = vector<Byte> (32, (Byte) 0xEA);

        options.disk.formatName = "prodos";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (io.Exists ("boot.dsk"));
    }

    //  The builder names exactly one reason and the runner passes it through,
    //  rather than restating the window arithmetic a second time.
    TEST_METHOD (DirectBoot_OutsideTheWindow_ReportsTheBuildersOwnReason)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeDirectBoot ("boot.dsk", "prog.bin");
        DiskCommandResult   result;

        io.files["prog.bin"] = vector<Byte> (32, (Byte) 0xEA);

        options.disk.loadAddress    = 0x0800;
        options.disk.hasLoadAddress = true;
        result                      = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("$0900") != std::string::npos,
                        L"the window's lower edge is named");
        Assert::IsFalse (io.Exists ("boot.dsk"));
    }

    //  IT ASKS FOR THE PAGE RATHER THAN LISTING THE COMMANDS ITSELF.
    //
    //  The refusal used to name all twelve, which put the same list on the
    //  screen twice once the edge started printing the disk page above it.
    //  What the refusal owes the reader now is which word it could not read;
    //  what the commands ARE is the page's job, and a sweep over the grammar's
    //  own table already holds the page to it.
    ////////////////////////////////////////////////////////////////////////////
    //
    //  create and init.
    //
    //  The commands that make a disk rather than edit one, and the pair the worked
    //  example needed: every step of it began `disk put mydisk.dsk`, and
    //  nothing anywhere made mydisk.dsk.
    //
    ////////////////////////////////////////////////////////////////////////////

    static CommandLineOptions MakeCreate (const char * path)
    {
        CommandLineOptions  options;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.command      = CommandLineOptions::DiskOptions::Command::Create;
        options.disk.commandWord  = "create";
        options.disk.imagePath = path;

        return options;
    }

    TEST_METHOD (Create_WritesAFormattedImageThatListsAsEmpty)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result = runner.Run (MakeCreate ("new.dsk"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (io.Exists ("new.dsk"), L"the image is there afterwards");
        Assert::IsTrue (result.output.find ("DOS 3.3") != std::string::npos,
                        L"and it says what it made");
    }

    //  IT WILL NOT WRITE OVER SOMETHING. A disk somebody still wanted is one
    //  keystroke from a disk they no longer have, and the refusal names the
    //  command for meaning it.
    TEST_METHOD (Create_RefusesToReplaceAnImageThatIsAlreadyThere)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  first  = runner.Run (MakeCreate ("new.dsk"));
        DiskCommandResult  second = runner.Run (MakeCreate ("new.dsk"));

        Assert::AreEqual (DiskCommandResult::kClean,    first.exitStatus);
        Assert::AreEqual (DiskCommandResult::kNoOutput, second.exitStatus);
        Assert::IsTrue (second.diagnostics.find ("init") != std::string::npos,
                        L"and points at the command that does mean it");
    }

    //  The container follows the name when --type is not given, which is what
    //  makes `disk create mydisk.po` do the obvious thing.
    TEST_METHOD (Create_TakesTheContainerFromTheNameWhenTypeIsNotGiven)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("new.po");
        DiskCommandResult   result;

        options.disk.formatName = "prodos";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find ("ProDOS") != std::string::npos);
    }

    //  A word that names no container is refused BY NAME. Handing back a .dsk
    //  would be a disk they did not ask for under a name they did.
    TEST_METHOD (Create_RefusesAContainerItCannotWrite)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("new.2mg");
        DiskCommandResult   result;

        options.disk.containerType = "2mg";
        result                     = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine, L"so the page prints above it");
        Assert::IsTrue (result.diagnostics.find ("2mg") != std::string::npos,
                        L"and the word they typed is quoted back");
        Assert::IsFalse (io.Exists ("new.2mg"), L"and nothing was written");
    }

    //
    //  EVERY WORD THE TOOL ADVERTISES MUST MAKE A DISK. The list is swept
    //  rather than typed out again, so a container added to the table without
    //  a matching arm in the builder fails here instead of raising an
    //  assertion dialog in front of whoever typed the new word -- which is
    //  exactly what `disk create foo.do` did, in both spellings, while the
    //  tool's own error text offered it.
    //
    //  Raw contents for the sweep, because that is the one filling every
    //  container takes; asking each for its own filesystem would mean
    //  restating the pairing matrix here, which is the thing that went stale.
    //
    TEST_METHOD (Create_WritesEveryContainerItAdvertises)
    {
        const DiskCommandRunner::ContainerName *  containers = nullptr;
        size_t                                    count      = 0;
        size_t                                    i          = 0;



        containers = DiskCommandRunner::GetAdvertisedContainers (count);

        Assert::IsTrue (count > 0, L"the tool advertises at least one container");

        for (i = 0; i < count; i++)
        {
            FakeDiskFileIo      io;
            DiskCommandRunner   runner (io);
            std::string         word    = containers[i].name;
            std::string         byName  = "byname." + word;
            std::string         byType  = "bytype." + word;
            std::wstring        which   = std::wstring (word.begin(), word.end());
            CommandLineOptions  options = MakeCreate (byName.c_str());
            DiskCommandResult   result;

            //  The name decides the container.
            options.disk.formatName = "none";
            result                  = runner.Run (options);

            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
                (L"an advertised extension must write a disk: ." + which).c_str());
            Assert::IsTrue (io.Exists (byName), L"and leave the image behind");

            //  And so does the word, said outright.
            options                    = MakeCreate (byType.c_str());
            options.disk.formatName    = "none";
            options.disk.containerType = word;
            result                     = runner.Run (options);

            Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
                (L"and so must the same word given to the type flag: " + which).c_str());
            Assert::IsTrue (io.Exists (byType), L"and leave that image behind too");
        }
    }

    //
    //  AN ADVERTISED CONTAINER ASKED FOR THE WRONG FILESYSTEM IS REFUSED, NOT
    //  ASSERTED. The builder's rules answer in verdicts for this reason: every
    //  one of them is reachable by typing, so E_INVALIDARG -- which means a
    //  caller has a bug, and asserts to say so -- is the wrong verdict here.
    //
    //  No ExpectedEhmAssert guard on purpose. An assertion inside this call
    //  routes to Assert::Fail, so the test fails if one fires.
    //
    TEST_METHOD (Create_RefusesTheWrongFilesystemForAContainerWithoutAsserting)
    {
        const DiskCommandRunner::ContainerName *  containers = nullptr;
        size_t                                    count      = 0;
        size_t                                    i          = 0;



        containers = DiskCommandRunner::GetAdvertisedContainers (count);

        for (i = 0; i < count; i++)
        {
            FakeDiskFileIo      io;
            DiskCommandRunner   runner (io);
            std::string         word     = containers[i].name;
            std::string         dosPath  = "dos." + word;
            std::string         proPath  = "prodos." + word;
            std::wstring        which    = std::wstring (word.begin(), word.end());
            CommandLineOptions  options  = MakeCreate (dosPath.c_str());
            DiskCommandResult   dos;
            DiskCommandResult   proDos;

            options.disk.formatName = "dos33";
            dos                     = runner.Run (options);

            options                 = MakeCreate (proPath.c_str());
            options.disk.formatName = "prodos";
            proDos                  = runner.Run (options);

            //  One of the two is refused for every container but woz, and the
            //  refusal is an ordinary exit rather than a broken invariant.
            Assert::IsTrue (dos.exitStatus    == DiskCommandResult::kClean
                         || dos.exitStatus    == DiskCommandResult::kNoOutput,
                (L"a DOS 3.3 disk is written or refused, never anything else: " + which).c_str());

            Assert::IsTrue (proDos.exitStatus == DiskCommandResult::kClean
                         || proDos.exitStatus == DiskCommandResult::kNoOutput,
                (L"and so is a ProDOS one: " + which).c_str());
        }
    }

    //  THE REFUSAL REPORTS THE RULE THAT WAS BROKEN, AND ONLY THAT ONE. The
    //  message this replaced recited the whole pairing matrix and the boot
    //  rule together, so it read the same whichever one had been tripped.
    TEST_METHOD (Create_RefusalReportsOnlyTheRuleThatWasBroken)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("wrong.po");
        DiskCommandResult   pairing;
        DiskCommandResult   badName;

        options.disk.formatName = "dos33";
        pairing                 = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, pairing.exitStatus);
        Assert::IsTrue (pairing.diagnostics.find ("illegal container and filesystem") != std::string::npos,
                        L"the broken rule is the one reported");
        Assert::IsTrue (pairing.diagnostics.find (".po holds ProDOS") != std::string::npos,
                        L"and the rule is spelled out");
        Assert::IsFalse (io.Exists ("wrong.po"), L"and nothing was written");

        options                 = MakeCreate ("badname.po");
        options.disk.formatName = "prodos";
        options.disk.volumeName = "1LEADINGDIGIT";
        badName                 = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, badName.exitStatus);
        Assert::IsTrue (badName.diagnostics.find ("illegal volume name") != std::string::npos,
                        L"the volume-name rule is the one reported");
        Assert::IsTrue (badName.diagnostics.find ("holds DOS 3.3") == std::string::npos,
                        L"and the pairing rule, which they did not break, stays out of it");

        //  A REFUSAL IS READ IN THE SAME TERMINAL A LISTING IS. A line that
        //  wraps loses the indent that marks it as the explanation.
        AssertEveryLineFitsEightyColumns (pairing.diagnostics);
        AssertEveryLineFitsEightyColumns (badName.diagnostics);
    }

    //  WHAT IS REPORTED IS WHAT IS ON THE DISK. ProDOS holds a volume name in
    //  upper case, so `--volume mydisk` makes /MYDISK and asking for it that
    //  way is perfectly good. The confirmation used to read back the name that
    //  was typed while `disk list` read back the name that is there, so the
    //  two disagreed over a disk that had been written correctly all along.
    TEST_METHOD (Create_ReportsTheProDosVolumeInTheCaseItIsStored_NotTheCaseItWasTyped)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("new.po");
        DiskCommandResult   result;

        options.disk.formatName = "prodos";
        options.disk.volumeName = "mydisk";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        Assert::IsTrue (result.output.find ("MYDISK") != std::string::npos,
                        L"the confirmation names the volume the disk actually has");

        Assert::IsTrue (result.output.find ("mydisk") == std::string::npos,
                        L"and never the lower-case form, which is on no disk anywhere");
    }




    //  A DOS 3.3 volume is a NUMBER, and a word that is not one is refused
    //  rather than quietly reading as zero.
    TEST_METHOD (Create_RefusesAVolumeThatIsNotADosNumber)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("new.dsk");
        DiskCommandResult   result;

        options.disk.volumeName = "MYDISK";
        result                  = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (io.Exists ("new.dsk"));
    }

    //  init needs one to be there; create needs one not to be. Between them
    //  every state is covered and neither guesses.
    TEST_METHOD (Init_RefusesAnImageThatIsNotThere)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("missing.dsk");
        DiskCommandResult   result;

        options.disk.command     = CommandLineOptions::DiskOptions::Command::Init;
        options.disk.commandWord = "init";
        result                = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("create") != std::string::npos,
                        L"and points at the command that makes one");
    }

    //  THE CONTAINER IS NOT A CHOICE UNDER init. It was decided when the file
    //  was made, so --type is refused rather than silently ignored.
    TEST_METHOD (Init_RefusesTypeBecauseTheContainerIsAlreadyDecided)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeCreate ("new.dsk");
        DiskCommandResult   result;

        runner.Run (options);

        options.disk.command          = CommandLineOptions::DiskOptions::Command::Init;
        options.disk.commandWord      = "init";
        options.disk.containerType = "woz";
        result                     = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine);
    }

    TEST_METHOD (UnknownCommand_AsksForThePageInsteadOfListingTheCommandsAgain)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Command::None);
        DiskCommandResult   result;

        options.disk.commandWord = "frobnicate";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine,
            L"which is what makes the edge print the page");
        Assert::IsTrue (result.diagnostics.find ("frobnicate") != std::string::npos,
            L"and the refusal names the word that could not be read");
        Assert::IsTrue (result.diagnostics.find ("catalog") == std::string::npos,
            L"without repeating the page's own command list under it");
    }

    //
    //  A REFUSED COMMAND LINE RUNS NOTHING, which is the half of the fix that
    //  lives here rather than in the parser.
    //
    //  The parser learned to refuse an option this grammar does not have --
    //  `-o`, which belongs to as65 -- and a refusal nothing acted on would have
    //  been a diagnostic on the screen and a clean exit status in the script.
    //  It matters most on a write: an option misread could have named the type
    //  the catalog records or the address a binary loads at, so the disk must
    //  not be edited on terms nobody asked for.
    //
    TEST_METHOD (RefusedCommandLine_RunsNothing_AndReportsNoOutput)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Command::List);
        DiskCommandResult   result;

        SeedRealDisk (io);
        options.parseVerdict = CommandLineOptions::ParseVerdict::Refused;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.output.empty(), L"a listing this command line did not earn");

        //  And nothing is said, because the parser already named the argument
        //  it could not take. One mistake, one message.
        Assert::AreEqual (std::string(), result.diagnostics);
    }

    //
    //  EVERY COLUMN THE VOLUME RECORDS, WITH NO FLAG TO ASK FOR IT.
    //
    //  `eof=` and `aux=` sat behind `--long`, and ProDosVolume::Enumerate fills
    //  both whether or not anybody asks -- so the flag bought nothing and cost
    //  a reading of the help plus a second run of the command. They are the two
    //  fields a build loop most wants, which is what made hiding them worst:
    //  the exact length of a file and the address a binary loads at are exactly
    //  what you check after placing one.
    //
    TEST_METHOD (List_OfAProDosVolume_CarriesEofAndAux_WithNoFlagToAskForThem)
    {
        static constexpr const char *  kProDosImage = "C:\\disks\\pro.dsk";

        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                                   kProDosImage);
        DiskCommandResult   result;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProDosImage);

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        //  A whole rendered row rather than a substring search for `eof=`: the
        //  columns have to land in their places, and the row has to stay inside
        //  80 characters now that nothing can turn them off.
        Assert::IsTrue (result.output.find ("*MERLIN.SYSTEM        $FF    37  eof=18432 aux=$2000\n")
                            != std::string::npos,
            L"the ProDOS row, with both columns, unasked for");

        AssertEveryLineFitsEightyColumns (result.output);
    }

    //  DOS 3.3 records neither field and has its own formatter, so it must be
    //  untouched by the ProDOS columns becoming unconditional.
    TEST_METHOD (List_OfADos33Volume_IsUnchangedByTheProDosColumns)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (result.output.find (" B 004 MERLIN\n") != std::string::npos,
            L"the DOS 3.3 row is what a booted machine prints");
        Assert::IsTrue (result.output.find ("eof=") == std::string::npos,
            L"and carries no field this filesystem does not record");
    }

    TEST_METHOD (Get_WithText_ConvertsFromAppleTextToHostText)
    {
        // T.SENDMSG is a real type-T file: high-bit ASCII with $8D line
        // terminators. Asked for as text it must arrive as something a host
        // editor opens, and asked for verbatim it must arrive untouched.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions  options   = MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult   verbatim;
        DiskCommandResult   converted;
        size_t              highBytes = 0;
        size_t              newlines  = 0;
        size_t              i         = 0;

        SeedRealDisk (io);
        options.disk.path = "T.SENDMSG";

        verbatim = runner.Run (options);

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;
        converted             = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, verbatim.exitStatus);
        Assert::AreEqual (DiskCommandResult::kClean, converted.exitStatus);

        for (i = 0; i < verbatim.payload.size(); i++)
        {
            if (verbatim.payload[i] >= 0x80) { highBytes++; }
        }

        Assert::IsTrue (highBytes > 0,
            L"the stored file must be high-bit for the conversion to mean anything");

        for (i = 0; i < converted.payload.size(); i++)
        {
            Assert::IsTrue (converted.payload[i] < 0x80,
                L"converted text carries no high-bit bytes");

            if (converted.payload[i] == '\n') { newlines++; }
        }

        Assert::IsTrue (newlines > 0, L"and its line endings are the host's");

        Assert::IsFalse (verbatim.payload == converted.payload,
            L"the two encodings must not deliver the same bytes -- otherwise the flag did nothing");
    }

    TEST_METHOD (Get_WithBasicOnAFileThatIsNoProgram_IsRefusedRatherThanRenderingGarbage)
    {
        // A source file is not a tokenized program, and the failure this forbids
        // is a listing rendered out of bytes that are not one: it would look
        // like a successful conversion and tokenize back to something else
        // entirely. Refusing needs the structural checks to be real -- walking
        // by the terminator alone would happily produce a listing here.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path     = "T.SENDMSG";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"nothing may be delivered under a conversion not performed");
        Assert::IsTrue (result.diagnostics.find ("--basic") != std::string::npos,
            L"and the refusal must name the flag it is refusing");

        AssertNamesNoPlatformCode (result.diagnostics);
    }

    //
    //  ------------------------------------------------------------------
    //  The commit path.
    //
    //  Its failure modes are the ones that never run in normal use, so they
    //  are the ones nothing has checked. The assertion that carries every
    //  test below is that the TARGET IS BYTE-IDENTICAL to what it was, plus
    //  that no temporary is left behind -- never merely that an error came
    //  back. An error verdict is satisfied by a half-written image.
    //  ------------------------------------------------------------------
    //

    //  The image exactly as it was seeded, kept for comparison. Read from the
    //  fixture rather than from the fake, so a commit that corrupted the fake's
    //  copy cannot also corrupt the oracle.
    vector<Byte> OriginalImageBytes()
    {
        FixtureProvider  fixtures;
        vector<Byte>     bytes;

        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proDos2.23.dsk", bytes));
        Assert::IsTrue (bytes.size() > 0, L"the oracle must actually have been read");

        return bytes;
    }

    void AssertImageIsUntouched (FakeDiskFileIo & io)
    {
        vector<Byte>  original = OriginalImageBytes();
        size_t        i        = 0;

        Assert::AreEqual (size_t (1), io.files.count (kImage), L"the image must still be there");
        Assert::AreEqual (original.size(), io.files[kImage].size(),
            L"and be the size it was");

        for (i = 0; i < original.size(); i++)
        {
            if (io.files[kImage][i] != original[i])
            {
                Assert::Fail (L"the image differs from what it was before the refused commit");
            }
        }

        Assert::IsTrue (io.HasNoTemporaryFiles(),
            L"and no temporary may be left beside it");
    }

    //  Something plausible to commit: the image with one byte changed, which is
    //  enough to prove a commit either landed or did not.
    vector<Byte> EditedImageBytes()
    {
        vector<Byte>  bytes = OriginalImageBytes();

        bytes[0] = (Byte) (bytes[0] ^ 0xFF);

        return bytes;
    }

    //  The temporary path a commit chose, taken from what the fake was asked to
    //  write rather than re-derived here -- a test that restates the derivation
    //  agrees with the code by construction and cannot disagree with it.
    std::string TemporaryPathChosen (const FakeDiskFileIo & io)
    {
        for (const std::string & path : io.writtenPaths)
        {
            if (path != kImage)
            {
                return path;
            }
        }

        Assert::Fail (L"nothing was written anywhere but the image");

        return std::string();
    }

    TEST_METHOD (Commit_OnTheHappyPath_ReplacesTheImageAndLeavesNoTemporary)
    {
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;
        vector<Byte>                   edited;

        SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (kImage, opened, result));

        edited = EditedImageBytes();

        AssertSucceeded (runner.GetSession().CommitImage (opened, edited, result));

        Assert::IsTrue (io.files[kImage] == edited, L"the new bytes are the image now");
        Assert::AreEqual (1, io.replaceCount, L"and arrived by one atomic replace");
        Assert::IsTrue (io.HasNoTemporaryFiles(), L"with nothing left over");
        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
    }

    TEST_METHOD (Commit_NeverWritesTheTargetDirectly)
    {
        // The property the whole plan exists for: the new bytes go somewhere
        // they cannot be mistaken for the image, and become the image in one
        // step. A commit that wrote the target and then tidied up would pass
        // every before-and-after comparison and still truncate the user's disk
        // when it was interrupted.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;

        SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (kImage, opened, result));
        AssertSucceeded (runner.GetSession().CommitImage (opened, EditedImageBytes(), result));

        for (const std::string & path : io.writtenPaths)
        {
            Assert::IsFalse (path == kImage,
                L"the image itself must never be handed to a plain write");
        }

        Assert::AreEqual (size_t (1), io.writtenPaths.size(),
            L"one write, to the temporary");
    }

    TEST_METHOD (Commit_WhenTheWriteFails_LeavesTheImageByteIdenticalAndSweepsThePartial)
    {
        // The fake leaves a partial file behind on a failed write, because the
        // platform does. Asserting only that an error came back would pass with
        // that partial file still sitting beside the user's disk.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;
        HRESULT                        hr     = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (kImage, opened, result));

        io.failNextWrite = true;

        hr = runner.GetSession().CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (1, io.removeCount, L"the partial temporary was swept");
        Assert::AreEqual (0, io.replaceCount, L"and nothing was ever put over the image");
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (kImage) != std::string::npos,
            L"and the refusal names the image");
    }

    TEST_METHOD (Commit_WhenTheReplaceFails_LeavesTheImageByteIdenticalAndRemovesTheTemporary)
    {
        // Here the temporary genuinely exists and is complete -- the last and
        // most tempting moment to leave it behind, since the bytes in it are
        // good and somebody might want them.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;
        HRESULT                        hr     = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (kImage, opened, result));

        io.failNextReplace = true;

        hr = runner.GetSession().CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (1, io.replaceCount, L"the replace was attempted");
        Assert::AreEqual (1, io.removeCount,  L"and its temporary removed when it failed");
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
    }

    TEST_METHOD (Commit_WhenTheImageWasRewrittenSinceItWasRead_RefusesBeforeWritingAnything)
    {
        // Somebody else landed a write between the read and the commit. What we
        // computed describes an image that no longer exists, so committing it
        // would silently discard their work.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;
        HRESULT                        hr     = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (kImage, opened, result));

        io.mutateStampOnNextStat = true;

        hr = runner.GetSession().CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount,   L"nothing was written at all");
        Assert::AreEqual (0, io.replaceCount, L"and nothing replaced");
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("changed since it was read") != std::string::npos,
            L"and the reason given is staleness, not some other refusal");
    }

    TEST_METHOD (Commit_WhenOnlyTheSIZEChangedSinceItWasRead_RefusesToo)
    {
        // The case the modification time cannot produce: filesystem timestamps
        // are coarse, so a second write inside one tick carries the same time
        // and only the size gives it away. A staleness check comparing time
        // alone passes every other test in this file and fails here.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;
        HRESULT                        hr     = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (kImage, opened, result));

        io.stamps[kImage].sizeBytes += 1;

        hr = runner.GetSession().CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount, L"nothing was written at all");
        Assert::IsTrue (result.diagnostics.find ("changed since it was read") != std::string::npos);
    }

    TEST_METHOD (Commit_WhenAnotherProgramHoldsTheImageOpen_RefusesWithoutTouchingIt)
    {
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;
        HRESULT                        hr     = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (kImage, opened, result));

        io.reportHeldByOther = true;

        hr = runner.GetSession().CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount,   L"the probe refuses before anything exists");
        Assert::AreEqual (0, io.replaceCount);
        Assert::AreEqual (0, io.removeCount,  L"so there is nothing to clean up either");
        Assert::IsTrue (result.diagnostics.find ("another program") != std::string::npos,
            L"and the reason given is the holder, not staleness or a write failure");
    }

    TEST_METHOD (Commit_WithNoStampRecordedAtRead_RefusesRatherThanSkippingTheCheck)
    {
        // A platform that cannot report a size and time leaves the staleness
        // guarantee unenforceable. Proceeding anyway would be indistinguishable
        // from enforcing it, which is the shape this codebase keeps getting
        // bitten by -- a degraded state that reads as a healthy one.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;
        HRESULT                        hr     = S_OK;

        SeedRealDisk (io);
        io.stamps.erase (kImage);

        AssertSucceeded (runner.GetSession().OpenImage (kImage, opened, result),
            L"reading does not need the stamp and must still work");

        Assert::IsFalse (opened.stampRecorded);

        hr = runner.GetSession().CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount);
        Assert::IsTrue (result.diagnostics.find ("could not be checked") != std::string::npos);
    }

    TEST_METHOD (Commit_WhenTheFirstTemporaryNameIsTaken_StepsOverItRatherThanOverwritingIt)
    {
        // Somebody's abandoned temporary is sitting at the name this commit
        // would take. Its contents are not ours to destroy, and more to the
        // point, a commit that overwrote it would be a commit that could also
        // overwrite a LIVE one belonging to a concurrent invocation.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;
        std::string                    taken;
        vector<Byte>                   sentinel = { 'N', 'O', 'T', 'Y', 'O', 'U', 'R', 'S' };

        // Learn the name this runner reaches for, by watching it commit once.
        SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (kImage, opened, result));
        AssertSucceeded (runner.GetSession().CommitImage (opened, EditedImageBytes(), result));

        taken = TemporaryPathChosen (io);

        // Put the image back, park something at that name, and make the SAME
        // runner commit again -- same runner, so the same name comes up first.
        SeedRealDisk (io);
        io.files[taken]  = sentinel;
        io.stamps[taken] = FileStamp { sentinel.size(), 1 };

        {
            DiskImageSession::OpenedImage  second;
            DiskCommandResult              secondResult;

            AssertSucceeded (runner.GetSession().OpenImage (kImage, second, secondResult));
            AssertSucceeded (runner.GetSession().CommitImage (second, EditedImageBytes(), secondResult));
        }

        Assert::AreEqual (size_t (1), io.files.count (taken),
            L"the file already there must survive");

        Assert::IsTrue (io.files[taken] == sentinel,
            L"byte for byte -- stepping over a name means not writing it");

        Assert::IsTrue (io.files[kImage] == EditedImageBytes(),
            L"and the commit still landed, under another name");
    }

    TEST_METHOD (Commit_TwoInvocations_DoNotReachForTheSameTemporaryName)
    {
        // The concurrency requirement, tested without concurrency. Two runners
        // stand in for two tool runs against one image; if both derive the same
        // name, both find it free at the same instant and one silently commits
        // the other's bytes. Nothing about that needs threads to demonstrate.
        FakeDiskFileIo                 ioA;
        FakeDiskFileIo                 ioB;
        DiskCommandRunner              runnerA (ioA);
        DiskCommandRunner              runnerB (ioB);
        DiskImageSession::OpenedImage  openedA;
        DiskImageSession::OpenedImage  openedB;
        DiskCommandResult              resultA;
        DiskCommandResult              resultB;

        SeedRealDisk (ioA);
        SeedRealDisk (ioB);

        AssertSucceeded (runnerA.GetSession().OpenImage (kImage, openedA, resultA));
        AssertSucceeded (runnerB.GetSession().OpenImage (kImage, openedB, resultB));
        AssertSucceeded (runnerA.GetSession().CommitImage (openedA, EditedImageBytes(), resultA));
        AssertSucceeded (runnerB.GetSession().CommitImage (openedB, EditedImageBytes(), resultB));

        Assert::IsFalse (TemporaryPathChosen (ioA) == TemporaryPathChosen (ioB),
            L"two invocations against one image must not reach for one name");
    }

    //  The in-use probe used to be described by a paragraph of help text, and a
    //  test here read that paragraph. Both are gone: a user meets the probe by
    //  having a write refused, so the refusal is the whole explanation, and
    //  DiskFailureModeTests already drives the refusal end to end against the
    //  wording it now shares with the runner.

    TEST_METHOD (Get_WithText_ToANamedFile_ConvertsThereToo)
    {
        // The conversion belongs to the payload, not to the destination. A
        // caller who redirects to a file and a caller who pipes must get the
        // same bytes.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Command::Get);
        DiskCommandResult  piped;
        DiskCommandResult  written;

        SeedRealDisk (io);
        options.disk.path     = "T.SENDMSG";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        piped = runner.Run (options);

        options.disk.hostFile = "C:\\out.txt";
        written               = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, written.exitStatus);
        Assert::AreEqual (size_t (1), io.files.count ("C:\\out.txt"));
        Assert::IsTrue (io.files["C:\\out.txt"] == piped.payload,
            L"the same conversion, whichever way the bytes leave");
    }

    //
    //  ------------------------------------------------------------------
    //  put and delete.
    //
    //  These are the first commands that change a user's disk, so every refusal
    //  below asserts the image is byte-for-byte what it was rather than
    //  merely that an error came back -- an error verdict is satisfied by a
    //  half-written image.
    //  ------------------------------------------------------------------
    //

    static constexpr const char *  kBlankImage = "C:\\disks\\blank.dsk";
    static constexpr const char *  kHostFile   = "C:\\build\\prog.bin";
    static constexpr const char *  kProImage   = "C:\\disks\\merlin.po.dsk";
    static constexpr const char *  kProOrdered = "C:\\disks\\merlin.po";

    static constexpr Byte      kBlankVolumeNumber = 254;
    static constexpr size_t    kPayloadBytes      = 512;
    static constexpr Word      kLoadAddress       = 0x6000;

    //  Bigger than the 496 sectors a freshly formatted volume leaves free, so
    //  placement is refused for want of room rather than for any other reason.
    static constexpr size_t    kOversizedBytes    = 130000;

    //  Catalog geometry, restated here only for the two tests that reach into
    //  a catalog entry directly -- to lock a file, and to break its chain.
    static constexpr int       kVtocTrack          = 17;
    static constexpr int       kCatalogFirstSector = 15;

    static constexpr size_t    kEntryBase         = 0x0B;
    static constexpr size_t    kEntOffTsTrack     = 0x00;
    static constexpr size_t    kEntOffTsSector    = 0x01;
    static constexpr size_t    kEntOffType        = 0x02;
    static constexpr size_t    kTsOffNextTrack    = 0x01;
    static constexpr size_t    kTsOffNextSector   = 0x02;
    static constexpr Byte      kLockedBit         = 0x80;
    static constexpr Byte      kTrackOffTheVolume = 40;

    vector<Byte> MakeBlankDos33Image()
    {
        vector<Byte>  buffer (NibblizationLayer::kImageByteSize, 0);

        AssertSucceeded (Dos33Skeleton::Write (buffer, kBlankVolumeNumber));

        return buffer;
    }

    void SeedFile (FakeDiskFileIo & io, const char * path, const vector<Byte> & bytes)
    {
        io.files[path]  = bytes;
        io.stamps[path] = FileStamp { bytes.size(), 100 };
    }

    //  A payload nothing else on the disk could be mistaken for, so a read-back
    //  comparison fails loudly rather than matching some neighboring sector.
    vector<Byte> MakePayload (size_t count = kPayloadBytes)
    {
        vector<Byte>  bytes (count, 0);
        size_t        i     = 0;

        for (i = 0; i < count; i++)
        {
            bytes[i] = (Byte) ((i * 7 + 0x21) & 0xFF);
        }

        return bytes;
    }

    CommandLineOptions MakePutOptions (const char * image,
                                       const char * hostFile,
                                       const char * asName)
    {
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Command::Put, image);

        options.disk.hostFile = hostFile;

        if (asName != nullptr)
        {
            options.disk.path = asName;
        }

        return options;
    }

    CommandLineOptions MakeDeleteOptions (const char * image, const char * path)
    {
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Command::Delete, image);

        options.disk.path = path;

        return options;
    }

    //  Reads a file back through a FRESH runner over the committed image, which
    //  is the only way to prove the bytes went to the disk rather than merely
    //  through the runner that put them there.
    DiskCommandResult GetFromCommittedImage (FakeDiskFileIo & io,
                                             const char     * image,
                                             const char     * path,
                                             CommandLineOptions::DiskOptions::Encoding encoding
                                                 = CommandLineOptions::DiskOptions::Encoding::Verbatim)
    {
        DiskCommandRunner   reader (io);
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Command::Get, image);

        options.disk.path     = path;
        options.disk.encoding = encoding;

        return reader.Run (options);
    }

    std::string ListCommittedImage (FakeDiskFileIo & io, const char * image)
    {
        DiskCommandRunner  reader (io);

        return reader.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List, image)).output;
    }

    void AssertImageMatches (FakeDiskFileIo & io, const char * path, const vector<Byte> & expected)
    {
        size_t  i = 0;

        Assert::AreEqual (size_t (1), io.files.count (path), L"the image must still be there");
        Assert::AreEqual (expected.size(), io.files[path].size(), L"and be the size it was");

        for (i = 0; i < expected.size(); i++)
        {
            if (io.files[path][i] != expected[i])
            {
                Assert::Fail (L"the image differs from what it was before the refused write");
            }
        }

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"and no temporary may be left beside it");
    }

    static void AssertNamesNoPlatformCode (const std::string & diagnostics)
    {
        // FR-014's demand is that a refusal reads as a reason. A hexadecimal
        // HRESULT is the specific failure mode it forbids, and it is the shape
        // a message picks up the moment somebody formats `hr` into it.
        Assert::IsTrue (diagnostics.find ("0x") == std::string::npos,
            L"a refusal must not carry a raw platform code");
    }

    //  For the refusals that must state exactly one reason: a count of one
    //  newline is what separates a single sentence from a message that
    //  reported its cause and then carried on into something else.
    static size_t CountNewlines (const std::string & text)
    {
        size_t  newlines = 0;
        size_t  at       = 0;

        while ((at = text.find ('\n', at)) != std::string::npos)
        {
            newlines++;
            at++;
        }

        return newlines;
    }

    ////////////////////////////////////////////////////////////////////////////
    //
    //  WHAT put's THREE NAMING OPTIONS DEFAULT TO.
    //
    //  The help says all of this in a paragraph, and a test used to assert the
    //  paragraph. Quoting a sentence proves somebody wrote it, not that the
    //  command does it: the wording could be perfect while the default moved,
    //  and the test would pass. These assert the defaults themselves, so the
    //  page can be reworded freely and cannot become untrue.
    //
    ////////////////////////////////////////////////////////////////////////////

    TEST_METHOD (Put_WithNoNameGiven_TakesTheOneTheFileHasOnTheHost)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, "GREET", "");

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, "GREET",     MakePayload());

        options.disk.path           = "";
        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find ("GREET") != std::string::npos,
            L"the name on the disk is the name it had on the host");
    }

    //  A BINARY IS WHAT put ASSUMES, which matters because the type decides
    //  whether the guest can do anything with the file at all.
    ////////////////////////////////////////////////////////////////////////////
    //
    //  THE TYPE IS READ OFF THE FILE WHEN NOBODY NAMED ONE.
    //
    //  The help says so, which is the only reason this has to be true: a page
    //  that claims a behavior the code does not have is worse than a page
    //  that claims nothing.
    //
    ////////////////////////////////////////////////////////////////////////////

    //  An Applesoft program: a chain of lines, each a next-pointer, a line
    //  number, tokens, and a zero, ending on a zero pointer.
    static vector<Byte> MakeApplesoftProgram()
    {
        vector<Byte>  program;
        Word          start = 0x0801;
        Word          next  = (Word) (start + 7);

        //  10 END
        program.push_back ((Byte) (next & 0xFF));
        program.push_back ((Byte) (next >> 8));
        program.push_back (10);
        program.push_back (0);
        program.push_back (0x80);              // END
        program.push_back (0);

        //  20 END
        program.push_back (0);                 // the chain ends
        program.push_back (0);

        return program;
    }

    TEST_METHOD (Put_WithNoType_ReadsApplesoftOffTheFilesOwnBytes)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakeApplesoftProgram());

        options.disk.typeName       = "";
        options.disk.hasLoadAddress = false;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus,
            L"a program needs no address, so nothing is refused");

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" A ") != std::string::npos,
            L"the catalog records Applesoft, unasked");
    }

    //  AND ANYTHING IT DOES NOT RECOGNIZE IS STILL A BINARY. A detector that
    //  fired on arbitrary bytes would file a build's output where the guest
    //  cannot run it, which is worse than never guessing.
    TEST_METHOD (Put_WithNoType_LeavesUnrecognizedBytesABinary)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" B ") != std::string::npos,
            L"an assembled binary stays a binary");
    }

    //  A NAMED TYPE BEATS THE BYTES. The detector informs a default; it does
    //  not argue with the operator.
    TEST_METHOD (Put_WithATypeNamed_KeepsItEvenWhenTheBytesSayOtherwise)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakeApplesoftProgram());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" B ") != std::string::npos,
            L"--type B was asked for and --type B is what the catalog says");
    }

    TEST_METHOD (Put_WithNoTypeGiven_StoresABinary)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" B ") != std::string::npos,
            L"the catalog records a binary");
    }

    //  A DOS 3.3 BINARY WITHOUT AN ADDRESS IS REFUSED. The header carries the
    //  load address, so there is no binary to write without one.
    TEST_METHOD (Put_ABinaryWithNoAddress_IsRefusedRatherThanGuessing)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.hasLoadAddress = false;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a binary with nowhere to load is not written");
        Assert::IsFalse (result.diagnostics.empty(), L"and the refusal says so");
    }

    TEST_METHOD (Put_ABinaryOntoAFreshVolume_LandsAndReadsBackByteForByte)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        vector<Byte>        payload = MakePayload();

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"a placement with room, a legal name and an address is clean");

        Assert::IsTrue (io.HasNoTemporaryFiles(), L"and leaves nothing beside the image");

        // 512 bytes plus the four-byte load/length header DOS stores inside the
        // file is 516 bytes -- three data sectors -- and the track/sector list
        // is a fourth. The `B 002` that the task text and quickstart both
        // carried is the arithmetic for a payload of 252 bytes or fewer.
        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" B 004 PROG") != std::string::npos,
            L"the guest's own listing shape, with the sector count the file really occupies");

        readBack = GetFromCommittedImage (io, kBlankImage, "PROG");

        Assert::AreEqual (DiskCommandResult::kClean, readBack.exitStatus);
        Assert::IsTrue (readBack.payload == payload,
            L"the bytes on the disk are the bytes that went in");

        Assert::IsTrue (readBack.diagnostics.find ("$6000") != std::string::npos,
            L"and the load address survived the round trip");
    }

    //  The committed image decoded the way the DRIVE sees it -- laid down as
    //  nibbles and read back through the hardware interleave -- rather than
    //  through the path that wrote it. This is the unit-suite half of the
    //  guest-visible placement gate: a placement written through a wrong
    //  understanding reads back perfectly through the same wrong
    //  understanding, and only the drive's own decode or a booted guest can
    //  say otherwise. The booted guest stays in the scenario suite.
    TEST_METHOD (Put_TheCommittedImage_DecodesThroughTheDriveWithThePlacedFileIntact)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskImage           image;
        SectorDecodeReport  report;
        vector<Byte>        payload = MakePayload();
        vector<Byte>        decoded;
        FilePayload         placed;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus,
            L"the placement must succeed before there is anything to decode");

        AssertSucceeded (NibblizationLayer::NibblizeDsk (io.files[kBlankImage], image));
        AssertSucceeded (NibblizationLayer::Denibblize (image, DiskFormat::Dsk, decoded, report));

        Assert::IsTrue (decoded == io.files[kBlankImage],
            L"every sector of the committed image must come back off the drive as it "
            L"went on");

        {
            Dos33Volume  volume (decoded);

            AssertSucceeded (volume.Read (FilePath::Parse ("PROG"), placed));
        }

        Assert::IsTrue (placed.bytes == payload,
            L"and the placed file must read back byte for byte off the container the "
            L"drive presents");

        Assert::IsTrue (placed.hasLoadAddress, L"with a load address recorded");
        Assert::AreEqual (kLoadAddress, placed.loadAddress);
    }

    TEST_METHOD (PutVerbatim_RoundTripsTheFILEBytes_NotTheImageBytes)
    {
        // THE GATE FOR THE UNCONVERTED PATH, AND THE ASSERTION IS FILE EQUALITY.
        //
        // Image equality is the wrong check and would fail here for two reasons
        // that have nothing to do with character conversion. A DOS 3.3 file
        // occupies whole sectors, so the bytes past its recorded length are
        // whatever was there before; and a replacement reallocates, so the file
        // can land somewhere else entirely on a disk that is otherwise
        // unchanged. Neither says anything about whether the bytes were
        // perturbed. What must hold -- and all that must hold -- is that the
        // file comes back identical.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options   = MakePutOptions (kImage, kHostFile, "MAKE DUMP");
        DiskCommandResult   extracted;
        DiskCommandResult   result;
        DiskCommandResult   readBack;

        SeedRealDisk (io);

        extracted = GetFromCommittedImage (io, kImage, "MAKE DUMP");

        Assert::AreEqual (DiskCommandResult::kClean, extracted.exitStatus);
        AssertIsMakeDumpPayload (extracted.payload);

        SeedFile (io, kHostFile, extracted.payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = 0x9000;
        options.disk.hasLoadAddress = true;
        options.disk.encoding       = CommandLineOptions::DiskOptions::Encoding::Verbatim;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"replacing a file with its own contents must succeed");

        readBack = GetFromCommittedImage (io, kImage, "MAKE DUMP");

        Assert::AreEqual (DiskCommandResult::kClean, readBack.exitStatus);

        // The file, byte for byte, through a path that re-read the committed
        // image rather than trusting the runner that wrote it.
        AssertIsMakeDumpPayload (readBack.payload);
        Assert::IsTrue (readBack.payload == extracted.payload);

        // And the reason an image comparison could not stand in for that: the
        // file occupies four sectors, so its footprint on the disk is 1024
        // bytes while the file itself is 589. Comparing the footprint would
        // compare 431 bytes that are not the file.
        Assert::AreEqual (size_t (589), readBack.payload.size());
        Assert::IsTrue (ListCommittedImage (io, kImage).find ("B 004 MAKE DUMP\n") != std::string::npos,
            L"four sectors of footprint for a 589-byte file");
    }

    TEST_METHOD (PutVerbatim_ReusesTheSpaceItFreed_AssertedSeparatelyFromTheBytes)
    {
        // The sector-reuse question, kept apart from the byte question on
        // purpose. Conflating them is what makes an image comparison look like
        // a conversion check: this asserts the volume gave back exactly what it
        // took, which is about ALLOCATION, and says nothing about the contents.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kImage, kHostFile, "MAKE DUMP");
        DiskCommandResult   extracted;
        std::string         before;
        std::string         after;

        SeedRealDisk (io);

        before    = ListCommittedImage (io, kImage);
        extracted = GetFromCommittedImage (io, kImage, "MAKE DUMP");

        SeedFile (io, kHostFile, extracted.payload);

        options.disk.typeName       = "B";
        options.disk.loadAddress    = 0x9000;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        after = ListCommittedImage (io, kImage);

        Assert::IsTrue (before.find ("sectors free of 560") != std::string::npos,
            L"the listing must actually be reporting free space for this to mean anything");

        Assert::AreEqual (FreeSpaceLine (before), FreeSpaceLine (after),
            L"a file replaced by its own contents leaks nothing and grows nothing");
    }

    //  The trailing free-space report, which the listing separates from the
    //  entries with a blank line. Taken as text rather than reparsed, so a
    //  change of either number shows up in the failure message.
    static std::string FreeSpaceLine (const std::string & listing)
    {
        size_t  at = listing.rfind ("\n\n");

        Assert::IsTrue (at != std::string::npos, L"the listing must carry a free-space report");

        return listing.substr (at);
    }

    TEST_METHOD (Put_OverALockedFile_IsRefusedInTermsThatSayItIsLocked)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        committed;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        LockFirstCatalogEntry (io.files[kBlankImage]);
        committed = io.files[kBlankImage];

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("is locked on this volume") != std::string::npos,
            L"the refusal must say the file is locked, not merely that something failed");

        Assert::IsTrue (result.diagnostics.find ("PROG") != std::string::npos,
            L"naming the file");
        Assert::IsTrue (result.diagnostics.find (kBlankImage) != std::string::npos,
            L"and the image");

        Assert::AreEqual (size_t (1), CountNewlines (result.diagnostics),
            L"and as ONE reason: a second line means the refusal reported its cause and "
            L"then carried on far enough to trip over something else");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, committed);
    }

    TEST_METHOD (Put_WhenTheVolumeHasNoRoom_RefusesAndLeavesTheImageByteIdentical)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "BIG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload (kOversizedBytes));

        options.disk.typeName = "T";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("does not fit") != std::string::npos,
            L"and says the volume has no room, not something generic");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
        Assert::AreEqual (0, io.writeCount, L"nothing was written anywhere at all");
    }

    TEST_METHOD (Put_WithANameTheCatalogCannotStore_RefusesAndSaysWhatALegalNameIs)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "9LIVES");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("9LIVES") != std::string::npos,
            L"the message names the name that was refused");
        Assert::IsTrue (result.diagnostics.find ("starting with a letter") != std::string::npos,
            L"and says what a legal one looks like");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
    }

    //  A listing whose every interesting rule is exercised: a token form
    //  inside a string, a DATA payload with a colon in a quoted item, a REM that
    //  swallows one, and spacing that has to survive the trip unchanged.
    static std::string BasicListing()
    {
        return "10 REM  A SPACED REM\n"
               "20 PRINT \"PRINT AT TO\"\n"
               "30 DATA \"A:B\",C: PRINT 1\n"
               "40 FOR I = 1 TO 9: NEXT\n";
    }

    static vector<Byte> BasicListingBytes()
    {
        std::string  listing = BasicListing();

        return vector<Byte> (listing.begin(), listing.end());
    }

    TEST_METHOD (Put_ABasicListing_LandsTokenizedAndComesBackAsTheSameListing)
    {
        // The property that matters to a user is that the file they placed and
        // the file they get back are the same file. Asserting only that the
        // placement succeeded would pass against a tokenizer that stored the
        // listing verbatim under a BASIC type, which is exactly what the
        // refusal this replaces existed to prevent.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        std::string         returned;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   BasicListingBytes());

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" A 002 PROG") != std::string::npos,
            L"a listing placed with --basic lands under the Applesoft type without "
            L"anybody naming one, and the whole rendered row says so");

        readBack = GetFromCommittedImage (io, kBlankImage, "PROG",
                                         CommandLineOptions::DiskOptions::Encoding::Basic);

        Assert::AreEqual (DiskCommandResult::kClean, readBack.exitStatus);
        Assert::IsTrue (readBack.hasPayload, L"and must hand back the listing");

        returned.assign (readBack.payload.begin(), readBack.payload.end());

        Assert::AreEqual (std::string ("10  REM  A SPACED REM\n"
                                       "20  PRINT \"PRINT AT TO\"\n"
                                       "30  DATA \"A:B\",C: PRINT 1\n"
                                       "40  FOR I = 1 TO 9: NEXT\n"),
                          returned,
            L"the listing must come back with the same statements and the same payload "
            L"spacing -- the leading spaces are the ones LIST puts in front of a token");
    }

    TEST_METHOD (Put_ABasicListing_StoresTheTOKENIZEDForm_NotTheTextItWasGiven)
    {
        // The companion assertion to the round trip above, and the one it
        // cannot make: a tokenizer that did nothing at all would round-trip
        // perfectly through its own inverse.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   raw;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   BasicListingBytes());

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        raw = GetFromCommittedImage (io, kBlankImage, "PROG",
                                    CommandLineOptions::DiskOptions::Encoding::Verbatim);

        Assert::IsTrue (raw.payload.size() > 4, L"the stored file has a body");

        Assert::AreEqual ((int) 0xB2, (int) raw.payload[4],
            L"the first body byte of line 10 is the REM TOKEN, not the letter R");

        Assert::AreEqual ((int) 0x0801 + 20, (int) (raw.payload[0] | (raw.payload[1] << 8)),
            L"and the link is the absolute address of the next line, which is what a "
            L"guest follows and what a length-only tokenizer would get wrong");

        Assert::IsTrue (raw.payload.size() < BasicListingBytes().size(),
            L"tokenized is shorter than the text it came from, or nothing was tokenized");
    }

    TEST_METHOD (Put_AnUntokenizableListing_IsRefusedNamingTheLineAndQuotingIt)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();
        std::string         listing = "10 PRINT 1\n20 PRINT 2\n20 PRINT 3\n";

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   vector<Byte> (listing.begin(), listing.end()));

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);

        Assert::IsTrue (result.diagnostics.find ("line 20 ") != std::string::npos,
            L"the refusal names the offending line number");

        Assert::IsTrue (result.diagnostics.find ("20 PRINT 3") != std::string::npos,
            L"and quotes the line itself, because a number alone points at nothing in a "
            L"file numbered by tens");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
        Assert::AreEqual (0, io.writeCount);
    }

    TEST_METHOD (Put_ABasicListingOntoProDos_RecordsWhereAnApplesoftProgramLoads)
    {
        // The two filesystems record this in entirely different places: DOS 3.3
        // keeps a length inside the file and nothing else, while ProDOS puts the
        // load address in the directory entry's auxiliary type. A placement that
        // left the auxiliary type at zero would read back through our own reader
        // perfectly and tell the machine the program loads at $0000.
        static constexpr const char *  kProDosImage = "C:\\disks\\pro.dsk";

        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kProDosImage, kHostFile, "PROG");
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        std::string         listing;
        std::string         returned;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-b.dsk", kProDosImage);
        SeedFile (io, kHostFile, BasicListingBytes());

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);

        {
            DiskCommandRunner   reader (io);
            CommandLineOptions  listOptions = MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                                           kProDosImage);

            listing = reader.Run (listOptions).output;
        }

        Assert::IsTrue (listing.find (" PROG                 $FC     1  eof=71 aux=$0801") != std::string::npos,
            L"the whole rendered row: the BASIC type nobody named, the size, the exact "
            L"length of the tokenized program, and the address it loads at");

        readBack = GetFromCommittedImage (io, kProDosImage, "PROG",
                                          CommandLineOptions::DiskOptions::Encoding::Basic);

        returned.assign (readBack.payload.begin(), readBack.payload.end());

        Assert::AreEqual (std::string ("10  REM  A SPACED REM\n"
                                       "20  PRINT \"PRINT AT TO\"\n"
                                       "30  DATA \"A:B\",C: PRINT 1\n"
                                       "40  FOR I = 1 TO 9: NEXT\n"),
                          returned,
            L"and the listing comes back off ProDOS exactly as it comes back off DOS 3.3");
    }

    TEST_METHOD (Put_BasicWithALoadAddress_IsRefusedRatherThanIgnoringTheFlag)
    {
        // Applesoft keeps its program at $0801 and nowhere else, so an address
        // here cannot be honored. Accepting it and placing the file anyway
        // leaves the caller believing something about the result that is false.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   BasicListingBytes());

        options.disk.encoding       = CommandLineOptions::DiskOptions::Encoding::Basic;
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("$0801") != std::string::npos,
            L"and says where an Applesoft program does load");

        AssertImageMatches (io, kBlankImage, blank);
        Assert::AreEqual (0, io.writeCount);
    }

    TEST_METHOD (Put_ABinaryWithNoLoadAddress_SaysWhichFlagIsMissing)
    {
        // $0000 is a legal load address, so defaulting would be
        // indistinguishable from an answer. The refusal has to name the flag,
        // or the caller is left guessing which of several things was wrong.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName = "B";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("--load") != std::string::npos,
            L"the message must name the flag that would fix it");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
    }

    TEST_METHOD (Put_ToAWriteProtectedImage_SaysWriteProtectedRatherThanReportingACode)
    {
        // FR-014's harder half. Nothing about the image's CONTENTS says it may
        // not be written, so the volume layer computes a perfectly good result
        // and the platform denies access at the last step. Reported as a
        // generic failure, the user is left with two candidate causes and no
        // way to tell which.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        io.failNextReplace  = true;
        io.nextReplaceError = HRESULT_FROM_WIN32 (ERROR_ACCESS_DENIED);

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("write-protected") != std::string::npos,
            L"the reason given must be write protection, not a generic replace failure");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, blank);
    }

    TEST_METHOD (Put_WithText_WritesTheDiskConventionAndReadsBackAsHostText)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = MakePutOptions (kBlankImage, kHostFile, "NOTES");
        DiskCommandResult   stored;
        DiskCommandResult   asText;
        std::string         source   = "HELLO\nWORLD\n";
        vector<Byte>        hostText (source.begin(), source.end());
        size_t              i        = 0;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   hostText);

        options.disk.typeName = "T";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        stored = GetFromCommittedImage (io, kBlankImage, "NOTES");

        Assert::AreEqual (DiskCommandResult::kClean, stored.exitStatus);
        Assert::IsTrue (stored.payload.size() > 0, L"something must actually have been stored");

        for (i = 0; i < stored.payload.size(); i++)
        {
            Assert::IsTrue (stored.payload[i] >= 0x80,
                L"the stored file is in the disk's own high-bit convention");
            Assert::IsTrue (stored.payload[i] != 0x0A,
                L"and carries none of the host's line endings");
        }

        asText = GetFromCommittedImage (io, kBlankImage, "NOTES",
                                        CommandLineOptions::DiskOptions::Encoding::Text);

        Assert::IsTrue (asText.payload == hostText,
            L"host text placed and read back as text is the identity");
    }

    TEST_METHOD (Put_WithTextAndNoNamedType_TakesTheFilesystemsOwnTextType)
    {
        // A caller who named no type gets the one that matches the conversion
        // they asked for. Defaulting to a binary instead would refuse this
        // invocation outright, for want of the load address a binary needs --
        // which is a confusing way to be told the type was wrong.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "NOTES");
        std::string         source  = "HELLO\n";
        vector<Byte>        hostText (source.begin(), source.end());

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   hostText);

        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus,
            L"text with no named type needs no load address");

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" T 002 NOTES") != std::string::npos,
            L"and lands as the filesystem's text type");
    }

    //
    //  THE OTHER HALF OF THE DEFAULTING RULE, AND THE HALF THAT LOST A ROUTE IN.
    //
    //  `--text --verbatim` used to reach this branch -- the second flag
    //  cancelled the conversion, so the type fell back to the default and the
    //  file landed as DOS `B` where `--text` alone made it `T`. `--verbatim` is
    //  gone, so naming neither conversion is the only way here now, and this is
    //  what keeps the branch it leads to honest.
    //
    TEST_METHOD (Put_WithNoConversionAndNoNamedType_TakesTheFilesystemsOwnBinaryType)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::IsTrue (options.disk.encoding == CommandLineOptions::DiskOptions::Encoding::Verbatim,
            L"nothing was named, which is the case under test");

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find (" B 004 PROG") != std::string::npos,
            L"naming neither a type nor a conversion lands the filesystem's binary type");
    }

    TEST_METHOD (Put_WithTextThatHasNoAppleRepresentation_NamesTheOffendingByte)
    {
        // A smart quote pasted into a listing looks identical to a plain one in
        // an editor. "Somewhere in this file" is not something anybody can act
        // on, so the offset is the value of the refusal.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "NOTES");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();
        vector<Byte>        source  = { 'H', 'I', ' ', 'T', 'H', 0xE2, 'R', 'E' };

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   source);

        options.disk.typeName = "T";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("byte 5") != std::string::npos,
            L"the message points at the byte, not merely at the file");

        AssertImageMatches (io, kBlankImage, blank);
    }

    TEST_METHOD (Put_WithATypeThisFilesystemDoesNotHave_RefusesAndListsWhatItTakes)
    {
        // Defaulting instead would place the file under a type nobody asked for
        // and say nothing; the guest reports the mismatch much later, as a file
        // that will not load.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        blank   = MakeBlankDos33Image();

        SeedFile (io, kBlankImage, blank);
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName = "SYS";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("DOS 3.3 takes") != std::string::npos,
            L"and the refusal lists what this filesystem does take");

        AssertImageMatches (io, kBlankImage, blank);
    }

    TEST_METHOD (Put_WithNoAsName_TakesTheHostFilesOwnLastComponent)
    {
        // SEARCHING FOR THE NAME AS A SUBSTRING IS NOT ENOUGH HERE, and this
        // test was written that way first. A DOS 3.3 catalog name may hold
        // colons and backslashes, so the whole host path is a legal name -- and
        // it CONTAINS the leaf, so a substring assertion is satisfied by an
        // implementation that never stripped the directories at all. Measured
        // by mutation: the weak form passed against exactly that.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, "C:\\build\\sub\\PROG.BIN", nullptr);
        DiskCommandResult   result;
        std::string         listing;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, "C:\\build\\sub\\PROG.BIN", MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result  = runner.Run (options);
        listing = ListCommittedImage (io, kBlankImage);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        Assert::IsTrue (listing.find (" B 004 PROG.BIN\n") != std::string::npos,
            L"the on-disk name is the host file's own last component and nothing else");

        Assert::IsTrue (listing.find ("BUILD") == std::string::npos,
            L"and carries none of the host's directories with it");
    }

    TEST_METHOD (Put_OntoAProDosVolume_RecordsTheLoadAddressAsTheAuxiliaryType)
    {
        // Where a binary's load address lives is the one thing the two
        // filesystems disagree about: DOS writes it into the file's own first
        // four bytes and ProDOS records it in the directory entry instead.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options  = MakePutOptions (kProImage, kHostFile, "PROG");
        DiskCommandRunner   reader (io);
        DiskCommandResult   result;
        DiskCommandResult   readBack;
        CommandLineOptions  listing  = MakeOptions (CommandLineOptions::DiskOptions::Command::List, kProImage);
        vector<Byte>        payload  = MakePayload();

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProImage);
        SeedFile (io, kHostFile, payload);

        options.disk.typeName       = "BIN";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);

        Assert::IsTrue (reader.Run (listing).output.find ("aux=$6000") != std::string::npos,
            L"ProDOS records the load address in the entry, not in the file");

        readBack = GetFromCommittedImage (io, kProImage, "PROG");

        Assert::IsTrue (readBack.payload == payload,
            L"and the file itself carries no header of its own");
    }

    TEST_METHOD (Put_ToAProDosOrderedContainer_IsRenderedBackThroughTheSameReorder)
    {
        // A .po holds its sectors in ProDOS order, so an edited buffer has to
        // be turned back into that order on the way out. Handing the volume
        // layer's own buffer straight to the commit instead is INVISIBLE on a
        // .dsk, where the two are the same bytes -- every other test here would
        // stay green while the tool wrote a file no Apple II could read.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options    = MakePutOptions (kProOrdered, kHostFile, "PROG");
        DiskCommandResult   readBack;
        FixtureProvider     fixtures;
        vector<Byte>        dosOrdered;
        vector<Byte>        proOrdered;
        vector<Byte>        payload    = MakePayload();

        AssertSucceeded (fixtures.OpenFixture ("Disks/Merlin-proProdos2.33-a.dsk", dosOrdered));

        VolumeImage::DosLogicalToProDosFile (dosOrdered, proOrdered);

        Assert::IsFalse (proOrdered == dosOrdered,
            L"the two orders must actually differ, or this test proves nothing");

        SeedFile (io, kProOrdered, proOrdered);
        SeedFile (io, kHostFile,   payload);

        options.disk.typeName       = "BIN";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        readBack = GetFromCommittedImage (io, kProOrdered, "PROG");

        Assert::AreEqual (DiskCommandResult::kClean, readBack.exitStatus,
            L"the committed file must still be a ProDOS volume in ProDOS order");

        Assert::IsTrue (readBack.payload == payload);
    }

    //  Sets the lock bit on the first catalog entry, which is where a file
    //  placed onto a freshly formatted volume lands.
    static void LockFirstCatalogEntry (vector<Byte> & buffer)
    {
        size_t  at = Dos33Skeleton::SectorOffset (kVtocTrack, kCatalogFirstSector)
                   + kEntryBase + kEntOffType;

        buffer[at] = (Byte) (buffer[at] | kLockedBit);
    }

    //  Points the first entry's track/sector list at a track that is not on the
    //  volume, so a walk of its chain cannot reach the end.
    static void BreakFirstEntrysChain (vector<Byte> & buffer)
    {
        size_t  entryAt = Dos33Skeleton::SectorOffset (kVtocTrack, kCatalogFirstSector) + kEntryBase;
        int     track   = buffer[entryAt + kEntOffTsTrack];
        int     sector  = buffer[entryAt + kEntOffTsSector];
        size_t  listAt  = Dos33Skeleton::SectorOffset (track, sector);

        buffer[listAt + kTsOffNextTrack]  = kTrackOffTheVolume;
        buffer[listAt + kTsOffNextSector] = 0;
    }

    TEST_METHOD (Delete_RemovesTheFileFromTheCommittedImage)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        DiskCommandResult   readBack;

        SeedRealDisk (io);

        // The trailing newline matters: this disk also carries MAKE DUMP.S, and
        // a substring search would go on finding the removed file inside its
        // neighbor's name and report a delete that never happened as one that
        // did -- or, here, the reverse.
        Assert::IsTrue (ListCommittedImage (io, kImage).find ("MAKE DUMP\n") != std::string::npos,
            L"the file must be there before the delete for this to mean anything");

        result = runner.Run (MakeDeleteOptions (kImage, "MAKE DUMP"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::IsTrue (io.HasNoTemporaryFiles());

        Assert::IsTrue (ListCommittedImage (io, kImage).find ("MAKE DUMP\n") == std::string::npos,
            L"and it is gone from the image on disk, not merely from a buffer");

        Assert::IsTrue (ListCommittedImage (io, kImage).find ("MAKE DUMP.S\n") != std::string::npos,
            L"while the file whose name merely contains it is untouched");

        readBack = GetFromCommittedImage (io, kImage, "MAKE DUMP");

        Assert::AreEqual (DiskCommandResult::kNoOutput, readBack.exitStatus,
            L"and no longer resolves by name");
    }

    TEST_METHOD (Delete_AFileTheVolumeDoesNotHave_RefusesNamingImageAndFile)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = OriginalImageBytes();

        SeedRealDisk (io);

        result = runner.Run (MakeDeleteOptions (kImage, "NOSUCHFILE"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (kImage) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("is not on this volume") != std::string::npos,
            L"and says which of the refusals this is");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kImage, original);
    }

    TEST_METHOD (Delete_ALockedFile_IsRefusedInTermsThatSayItIsLocked)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        committed;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        LockFirstCatalogEntry (io.files[kBlankImage]);
        committed = io.files[kBlankImage];

        result = runner.Run (MakeDeleteOptions (kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("is locked on this volume") != std::string::npos);

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, committed);
    }

    TEST_METHOD (Delete_WhenTheChainCannotBeFollowed_SucceedsAndSaysWhatItCouldNotAccountFor)
    {
        // The reason the removal's ACCOUNT had to become part of the volume
        // interface. Delete stays available for a file whose chain is damaged,
        // so a bad entry cannot strand the volume -- but a caller that reported
        // only success would leave the user believing every sector came back.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        BreakFirstEntrysChain (io.files[kBlankImage]);

        result = runner.Run (MakeDeleteOptions (kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandResult::kWithComplaints, result.exitStatus,
            L"a removal that could not account for everything is not a clean one");

        Assert::IsTrue (result.diagnostics.find ("could not be followed") != std::string::npos,
            L"and says what it could not do, rather than only that something was wrong");

        Assert::IsTrue (ListCommittedImage (io, kBlankImage).find ("PROG") == std::string::npos,
            L"while still removing the entry, so a bad file cannot strand the volume");
    }

    //
    //  ------------------------------------------------------------------
    //  boot.
    //
    //  The one edit with nothing to show for itself in a listing: what a disk
    //  runs at boot is not a file and does not appear beside the files. So the
    //  assertions here read the bytes the mechanism actually uses, and the
    //  refusals are held to the same standard as put's and delete's -- image
    //  byte-for-byte as it was, no temporary, one reason in words.
    //  ------------------------------------------------------------------
    //

    //  Where a booted DOS reads the name of the program it runs. Restated from
    //  the published layout rather than taken from the implementation, and
    //  corroborated by this fixture: the Merlin DOS 3.3 disk carries the
    //  high-ASCII form of HELLO here, exactly as the stock master does.
    static constexpr int     kGreetingTrack  = 1;
    static constexpr int     kGreetingSector = 9;
    static constexpr size_t  kGreetingOffset = 0x75;
    static constexpr size_t  kNameFieldBytes = 30;

    //  The Applesoft program placed on the DOS 3.3 disk to be booted into, a
    //  binary that disk already carries, and a file on the Merlin ProDOS disk
    //  that its boot path could never launch.
    static constexpr const char *  kDosProgram      = "BOOTME";
    static constexpr const char *  kBinaryOnTheDisk = "MERLIN.X";
    static constexpr const char *  kProProgram      = "PARMS";

    static std::string GreetingNameIn (const vector<Byte> & image)
    {
        size_t       at   = Dos33Skeleton::SectorOffset (kGreetingTrack, kGreetingSector)
                          + kGreetingOffset;
        std::string  name;
        size_t       i    = 0;

        for (i = 0; i < kNameFieldBytes; i++)
        {
            name += (char) (image[at + i] & 0x7F);
        }

        while (!name.empty() && name.back() == ' ')
        {
            name.pop_back();
        }

        return name;
    }

    //  The raw thirty bytes of the field, not merely the name they spell: the
    //  name in high ASCII, padded with high-ASCII spaces to the width of a
    //  catalog name field, which is the shape a booting DOS matches against
    //  its own catalog. A field holding the right letters in the wrong
    //  encoding names a file DOS then fails to find.
    static void AssertGreetingFieldHolds (const vector<Byte> & image, const char * name)
    {
        size_t       at   = Dos33Skeleton::SectorOffset (kGreetingTrack, kGreetingSector)
                          + kGreetingOffset;
        std::string  text = name;
        size_t       i    = 0;

        for (i = 0; i < kNameFieldBytes; i++)
        {
            Byte  expected = (i < text.size()) ? (Byte) (text[i] | 0x80) : (Byte) 0xA0;

            Assert::AreEqual ((int) expected, (int) image[at + i],
                L"the greeting field must hold the name in high ASCII, space-padded to "
                L"the width of a catalog name field");
        }
    }

    CommandLineOptions MakeBootOptions (const char * image, const char * path)
    {
        CommandLineOptions  options = MakeOptions (CommandLineOptions::DiskOptions::Command::Boot, image);

        options.disk.path = path;

        return options;
    }

    TEST_METHOD (Boot_SetsTheNameTheGuestReadsAtBootTime_InTheCommittedImage)
    {
        // An Applesoft program, because that is what a booting DOS 3.3 RUNs.
        // The binary case is the one below, and it is a complaint rather than a
        // refusal.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  put = MakePutOptions (kImage, kHostFile, kDosProgram);
        DiskCommandResult   result;

        SeedRealDisk (io);
        SeedFile (io, kHostFile, MakePayload (16));

        put.disk.typeName = "A";

        Assert::AreEqual (std::string ("HELLO"), GreetingNameIn (io.files[kImage]),
            L"this disk boots its own greeting today, or the command below changes nothing");

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (put).exitStatus,
            L"the program has to be on the disk before it can be booted into");

        result = runner.Run (MakeBootOptions (kImage, kDosProgram));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus,
            L"naming a program the volume holds and DOS can run is a clean operation");

        Assert::AreEqual (std::string(), result.diagnostics, L"with nothing to complain about");
        Assert::IsTrue (io.HasNoTemporaryFiles(), L"and nothing left beside the image");

        Assert::AreEqual (std::string (kDosProgram), GreetingNameIn (io.files[kImage]),
            L"and the name the guest reads at boot is the one that was asked for -- read "
            L"back off the COMMITTED image, not out of a buffer the runner still held");

        AssertGreetingFieldHolds (io.files[kImage], kDosProgram);

        Assert::IsTrue (ListCommittedImage (io, kImage).find (" A 004 HELLO") != std::string::npos,
            L"while the greeting it used to run is still on the disk, untouched: this "
            L"changes what runs, not what is on the volume");
    }

    //  A STARTUP PROGRAM DOS 3.3 CANNOT RUN IS REFUSED, AND NOTHING IS WRITTEN.
    //
    //  Measured on the stock master with a real 6502: the disk boots and the
    //  binary is never executed, because DOS 3.3's boot command is RUN.
    //
    //  This used to set the name, commit the image and then say so, on the
    //  reasoning that a DOS patched by hand to BRUN is a real thing and
    //  refusing would block it. What that produced for everyone else was a
    //  command that reported trouble and changed the disk anyway, leaving a
    //  volume configured to start a program that cannot start -- which is
    //  what ProDOS refuses outright in the same command.
    TEST_METHOD (Boot_ABinaryOnADos33Volume_IsRefused_AndTheGreetingIsUntouched)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        std::string         before;

        SeedRealDisk (io);

        before = GreetingNameIn (io.files[kImage]);
        result = runner.Run (MakeBootOptions (kImage, kBinaryOnTheDisk));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a startup program DOS will not run is refused, not carried out");

        Assert::IsTrue (result.diagnostics.find ("RUN") != std::string::npos,
            L"and the refusal says what DOS does at boot");

        Assert::IsTrue (result.diagnostics.find (kBinaryOnTheDisk) != std::string::npos,
            L"naming the file it is about");

        Assert::AreEqual (before, GreetingNameIn (io.files[kImage]),
            L"and the greeting the disk already had is left exactly as it was");

        AssertNamesNoPlatformCode (result.diagnostics);
    }

    TEST_METHOD (Boot_ANameTheVolumeDoesNotHold_IsRefusedNamingTheMissingFile)
    {
        // The refusal FR-025 asks for, and the reason it is worth having: a
        // startup program that is not there produces no symptom until somebody
        // boots the disk, and then the symptom is on a machine somewhere else.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = OriginalImageBytes();

        SeedRealDisk (io);

        result = runner.Run (MakeBootOptions (kImage, "NOSUCHFILE"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("NOSUCHFILE") != std::string::npos,
            L"the message must name the file that is missing, not merely report a failure");
        Assert::IsTrue (result.diagnostics.find (kImage) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("is not on this volume") != std::string::npos);

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kImage, original);

        Assert::AreEqual (std::string ("HELLO"), GreetingNameIn (io.files[kImage]),
            L"and the disk still boots what it booted before");
    }

    //  A REQUIRED OPERAND THAT IS NOT THERE ANSWERS WITH THAT COMMAND'S USAGE.
    //
    //  This asserted the sentence "no program named", which pinned wording
    //  rather than behavior and stood in the way of saying it better. What
    //  matters is that the command is refused, that the reader is shown the
    //  usage for the command they asked about rather than for all eight, and
    //  that the image is not touched.
    TEST_METHOD (Boot_WithNoProgramGiven_ShowsThatCommandsUsage_AndTouchesNothing)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original = OriginalImageBytes();

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::Boot));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine, L"a missing operand is a bad command line");
        Assert::IsTrue (result.usageShown,     L"and the reader is shown how to write it");

        //  boot's block, and not the whole page: the grammar of the command
        //  asked about is there, and a command nobody asked about is not.
        Assert::IsTrue (result.output.find ("CassoCli disk boot <image> <name>") != std::string::npos,
            L"boot's own grammar");
        Assert::IsTrue (result.output.find ("CassoCli disk sectorwrite") == std::string::npos,
            L"and not every other command's");

        AssertImageMatches (io, kImage, original);
    }

    //  THE SAME ANSWER FOR A BAD VALUE AS FOR A MISSING OPERAND. These used to
    //  differ: `sectorread` with no image answered in 18 lines and
    //  `sectorread --track 99` in 194, the whole page, though both readers had
    //  named the command they wanted and differed only in how they got it
    //  wrong. Nothing pinned it, which is how the two drifted apart.
    TEST_METHOD (BadOptionValue_ShowsThatCommandsUsage_NotEveryOtherCommands)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedBlankImage (io, "raw.dsk");

        result = runner.Run (MakeSectorRead ("raw.dsk", 99, 0, 1, kLogical));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine, L"a value out of range is a bad command line");
        Assert::IsTrue (result.usageShown,     L"and the reader is shown how to write it");

        Assert::IsTrue (result.output.find ("CassoCli disk sectorread <image>") != std::string::npos,
            L"sectorread's own grammar");
        Assert::IsTrue (result.output.find ("CassoCli disk create") == std::string::npos,
            L"and not every other command's");
    }

    //  AND THE ONE CASE THAT MUST STAY WIDE. A word the grammar does not know
    //  leaves no command to narrow to, and a reader who has not landed on one
    //  is exactly who the whole page is for. Narrowing this would answer an
    //  unrecognized command with a block belonging to some other command.
    TEST_METHOD (AnUnknownCommand_StillAnswersWithTheWholePage)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options;
        DiskCommandResult   result;

        options.subcommand       = CommandLineOptions::Subcommand::Disk;
        options.disk.command     = CommandLineOptions::DiskOptions::Command::None;
        options.disk.commandWord = "frobnicate";
        options.disk.imagePath   = "raw.dsk";

        SeedBlankImage (io, "raw.dsk");

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.badCommandLine);

        Assert::IsTrue (result.diagnostics.find ("frobnicate") != std::string::npos,
            L"the word they typed is quoted back");

        //  usageShown is what tells the edge the runner already answered. Left
        //  false, the edge prints the whole page, which is the answer here.
        Assert::IsFalse (result.usageShown,
            L"no command to narrow to, so the edge prints the whole page");

        Assert::IsTrue (result.output.empty(),
            L"and the runner offers no block of its own to be printed instead");
    }

    TEST_METHOD (Boot_AVolumeWithNoOperatingSystemOnIt_IsRefusedInWordsRatherThanPatchedAnyway)
    {
        // A formatted data disk reserves the tracks DOS would occupy but has
        // nothing in them. Patching a name there would report success for a
        // disk that cannot boot at all.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kBlankImage, kHostFile, "PROG");
        DiskCommandResult   result;
        vector<Byte>        committed;

        SeedFile (io, kBlankImage, MakeBlankDos33Image());
        SeedFile (io, kHostFile,   MakePayload());

        options.disk.typeName       = "B";
        options.disk.loadAddress    = kLoadAddress;
        options.disk.hasLoadAddress = true;

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus);

        committed = io.files[kBlankImage];

        result = runner.Run (MakeBootOptions (kBlankImage, "PROG"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("no operating system") != std::string::npos,
            L"and says that is what is missing, rather than blaming the file");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kBlankImage, committed);
    }

    TEST_METHOD (Boot_ProDos_AFileTheBootPathCannotLaunch_IsRefusedInWords)
    {
        // On ProDOS the mechanism is directory order, so any file could be
        // moved to the front -- and moving one the kernel will never launch
        // produces a disk that looks configured and boots something else.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        vector<Byte>        original;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProImage);

        original = io.files[kProImage];

        result = runner.Run (MakeBootOptions (kProImage, kProProgram));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (kProProgram) != std::string::npos);
        Assert::IsTrue (result.diagnostics.find ("type SYS") != std::string::npos,
            L"and says what this boot path does launch");

        AssertNamesNoPlatformCode (result.diagnostics);
        AssertImageMatches (io, kProImage, original);
    }

    //  Which of two names the committed listing reaches first. Both must be
    //  present: comparing two positions where one of them is "not found" is a
    //  comparison against the largest number there is, which passes.
    void AssertListedBefore (const std::string & listing,
                             const std::string & first,
                             const std::string & second)
    {
        size_t  at     = listing.find (first);
        size_t  behind = listing.find (second);

        Assert::IsTrue (at     != std::string::npos, L"the first name must be in the listing");
        Assert::IsTrue (behind != std::string::npos, L"and so must the second");
        Assert::IsTrue (at < behind, L"in that order");
    }

    TEST_METHOD (Boot_ProDos_ASystemProgramPlacedByPut_OvertakesTheOneTheDiskLaunchesNow)
    {
        // /MERLIN launches MERLIN.SYSTEM, the first system program its volume
        // directory reaches. Nominating a newly placed one has to overtake it,
        // and the proof is the ORDER in the committed image rather than any
        // field, because ProDOS stores no startup name anywhere to inspect.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        CommandLineOptions  options = MakePutOptions (kProImage, kHostFile, "CASSO.SYSTEM");
        DiskCommandResult   result;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", kProImage);
        SeedFile (io, kHostFile, MakePayload (60));

        options.disk.typeName = "SYS";

        Assert::AreEqual (DiskCommandResult::kClean, runner.Run (options).exitStatus,
            L"the placement must succeed before the reorder can mean anything");

        AssertListedBefore (ListCommittedImage (io, kProImage), "MERLIN.SYSTEM", "CASSO.SYSTEM");

        result = runner.Run (MakeBootOptions (kProImage, "CASSO.SYSTEM"));

        Assert::AreEqual (DiskCommandResult::kClean, result.exitStatus);
        Assert::AreEqual (std::string(), result.diagnostics);
        Assert::IsTrue (io.HasNoTemporaryFiles());

        AssertListedBefore (ListCommittedImage (io, kProImage), "CASSO.SYSTEM", "MERLIN.SYSTEM");

        Assert::IsTrue (ListCommittedImage (io, kProImage).find ("PARMS") != std::string::npos,
            L"and every other entry is still on the disk");
    }


    //  A 143,360-byte sector image carrying boot code and no filesystem at all,
    //  which is what a great deal of Apple II software actually is.
    static vector<Byte> MakeBootableImageWithNoFilesystem()
    {
        vector<Byte>  image ((size_t) NibblizationLayer::kImageByteSize, 0);
        size_t        i     = 0;

        // Track 0 sector 0 is what the drive's ROM reads and jumps into.
        for (i = 0; i < 64; i++)
        {
            image[i] = static_cast<Byte> (0xA9 + (i & 0x0F));
        }

        return image;
    }

    TEST_METHOD (List_ImageWithNoFilesystem_SaysSoInPlainWords_AndThenSaysWhatItCanTell)
    {
        //  TWELVE OF FOURTEEN REAL IMAGES LAND HERE, this project's own demo
        //  disk among them, and every one of them boots. Answering with the
        //  negative alone describes a working disk as though it were unreadable.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        const char *        path = "C:\\disks\\casso-rocks.dsk";
        vector<Byte>        image = MakeBootableImageWithNoFilesystem();

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List, path));

        Assert::IsTrue (result.diagnostics.find (DiskImageSession::kNoFilesystemText)
                            != std::string::npos,
            L"the sentence a person would say, not a sentence about this tool's tables");

        Assert::IsTrue (result.diagnostics.find ("filesystem this tool recognizes")
                            == std::string::npos,
            L"and not the old one");

        Assert::IsTrue (result.diagnostics.find ("35 tracks x 16 sectors x 256 bytes")
                            != std::string::npos,
            L"the geometry is still knowable and is still worth stating");

        Assert::IsTrue (result.diagnostics.find ("track 0 sector 0 carries code")
                            != std::string::npos,
            L"and so is the fact that it boots");

        //  THE STATUS DOES NOT MOVE. A caller still got no catalog, so a script
        //  that branches on 2 branches the same way it always did.
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus,
            L"a survey is not a listing");

        Assert::AreEqual (std::string(), result.output,
            L"and it goes to the error stream, so nothing appears in a pipe");
    }

    TEST_METHOD (List_ImageWithNoFilesystemAndNoBootCode_SaysThatInsteadOfClaimingItBoots)
    {
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        const char *        path  = "C:\\disks\\blank.dsk";
        vector<Byte>        image ((size_t) NibblizationLayer::kImageByteSize, 0);

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List, path));

        Assert::IsTrue (result.diagnostics.find ("track 0 sector 0 is blank")
                            != std::string::npos,
            L"an empty first sector is reported as empty, not as bootable");
    }

    TEST_METHOD (Failure_EchoesANonAsciiPathByteForByte_BecauseItHasToBePastable)
    {
        //  The measured bug. `Space Quarks (1981)(Broderbund)(II-II+)[48K].woz`
        //  came back as `Br?derbund`, which is a name nobody can paste into a
        //  command line. The mangling happened at the console, not here -- so
        //  what this pins is that the RUNNER hands its caller the same bytes it
        //  was given, leaving exactly one place for the conversion to live.
        //
        //  The escape is split because a hex escape in C++ is greedy: "\xF8d"
        //  would be read as one character numbered $F8D.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;
        std::string         path  = "C:\\disks\\Space Quarks (1981)(Br\xF8" "derbund).woz";
        vector<Byte>        image ((size_t) NibblizationLayer::kImageByteSize, 0);

        io.files[path]  = image;
        io.stamps[path] = FileStamp { image.size(), 100 };

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                          path.c_str()));

        Assert::IsTrue (result.diagnostics.find (path) != std::string::npos,
            L"the name comes back exactly as it went in");

        Assert::IsTrue (result.diagnostics.find ('?') == std::string::npos,
            L"and with no substitution character anywhere in the message");
    }

    TEST_METHOD (Failure_NamesAnUnreadableImage_WithoutAssertingOnTheUsersInput)
    {
        //  A user naming an image that is not there is not a coding error, so
        //  the path must not be the one that asserts.
        FakeDiskFileIo      io;
        DiskCommandRunner   runner (io);
        DiskCommandResult   result;

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Command::List,
                                          "C:\\disks\\absent.dsk"));

        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("cannot be read") != std::string::npos);
    }
};
