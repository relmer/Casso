#include "Pch.h"
#include "../EhmTestHelper.h"
#include "FixtureProvider.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "Devices/Disk/NibblizationLayer.h"

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

    CommandLineOptions MakeOptions (CommandLineOptions::DiskOptions::Verb verb,
                                    const char * image = kImage)
    {
        CommandLineOptions  options;

        options.subcommand     = CommandLineOptions::Subcommand::Disk;
        options.disk.verb      = verb;
        options.disk.imagePath = image;

        return options;
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

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus,
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

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List));

        Assert::IsTrue (result.output.find ("sectors free of 560") != std::string::npos,
            L"the listing states free space against the volume's capacity");
    }

    TEST_METHOD (List_ProDosVolume_UsesItsOwnShape)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedRealDisk (io, "Disks/Merlin-proProdos2.33-a.dsk", "C:\\disks\\merlin.po.dsk");

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List,
                                          "C:\\disks\\merlin.po.dsk"));

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
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
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path = "MAKE DUMP";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
        Assert::IsTrue (result.hasPayload, L"the payload is returned for the caller to deliver");
        AssertIsMakeDumpPayload (result.payload);
        Assert::AreEqual (size_t (0), io.files.count ("C:\\out.bin"),
            L"and nothing was written to the host");
    }

    TEST_METHOD (Get_WithOutputFile_WritesThroughTheSeam)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path     = "MAKE DUMP";
        options.disk.hostFile = "C:\\out.bin";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
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
        CommandLineOptions  options   = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
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
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
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
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path = "NOSUCHFILE";

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
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

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
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

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::List));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("recognizes") != std::string::npos);
    }

    TEST_METHOD (UnbuiltVerb_ReportsFailureRatherThanDoingNothingQuietly)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        SeedRealDisk (io);

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::Put));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus,
            L"an absent capability must not look like a completed operation");
        Assert::IsTrue (result.diagnostics.size() > 0);
    }

    TEST_METHOD (UnknownVerb_SuggestsTheOnesThatExist)
    {
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        DiskCommandResult  result;

        result = runner.Run (MakeOptions (CommandLineOptions::DiskOptions::Verb::None));

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find ("list") != std::string::npos,
            L"a bad verb should say what the good ones are");
    }

    TEST_METHOD (Get_WithText_ConvertsFromAppleTextToHostText)
    {
        // T.SENDMSG is a real type-T file: high-bit ASCII with $8D line
        // terminators. Asked for as text it must arrive as something a host
        // editor opens, and asked for verbatim it must arrive untouched.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions  options   = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
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

        Assert::AreEqual (DiskCommandRunner::kClean, verbatim.exitStatus);
        Assert::AreEqual (DiskCommandRunner::kClean, converted.exitStatus);

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

    TEST_METHOD (Get_WithBasic_IsRefusedRatherThanQuietlyIgnored)
    {
        // The failure this forbids is specific: --basic parsed and then dropped
        // would hand back tokenized bytes while the help promises a listing, and
        // nothing in the output would distinguish that from a file needing no
        // conversion. A refusal is the honest answer until the tokenizer exists.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  result;

        SeedRealDisk (io);
        options.disk.path     = "T.SENDMSG";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Basic;

        result = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
        Assert::IsFalse (result.hasPayload, L"nothing may be delivered under a conversion not performed");
        Assert::IsTrue (result.diagnostics.find ("--basic") != std::string::npos,
            L"and the refusal must name the flag it is refusing");
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
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        vector<Byte>                   edited;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        edited = EditedImageBytes();

        AssertSucceeded (runner.CommitImage (opened, edited, result));

        Assert::IsTrue (io.files[kImage] == edited, L"the new bytes are the image now");
        Assert::AreEqual (1, io.replaceCount, L"and arrived by one atomic replace");
        Assert::IsTrue (io.HasNoTemporaryFiles(), L"with nothing left over");
        Assert::AreEqual (DiskCommandRunner::kClean, result.exitStatus);
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
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));
        AssertSucceeded (runner.CommitImage (opened, EditedImageBytes(), result));

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
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.failNextWrite = true;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (1, io.removeCount, L"the partial temporary was swept");
        Assert::AreEqual (0, io.replaceCount, L"and nothing was ever put over the image");
        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
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
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.failNextReplace = true;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (1, io.replaceCount, L"the replace was attempted");
        Assert::AreEqual (1, io.removeCount,  L"and its temporary removed when it failed");
        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
    }

    TEST_METHOD (Commit_WhenTheImageWasRewrittenSinceItWasRead_RefusesBeforeWritingAnything)
    {
        // Somebody else landed a write between the read and the commit. What we
        // computed describes an image that no longer exists, so committing it
        // would silently discard their work.
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.mutateStampOnNextStat = true;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount,   L"nothing was written at all");
        Assert::AreEqual (0, io.replaceCount, L"and nothing replaced");
        Assert::AreEqual (DiskCommandRunner::kNoOutput, result.exitStatus);
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
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.stamps[kImage].sizeBytes += 1;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (0, io.writeCount, L"nothing was written at all");
        Assert::IsTrue (result.diagnostics.find ("changed since it was read") != std::string::npos);
    }

    TEST_METHOD (Commit_WhenAnotherProgramHoldsTheImageOpen_RefusesWithoutTouchingIt)
    {
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));

        io.reportHeldByOther = true;

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

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
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        HRESULT                        hr = S_OK;

        SeedRealDisk (io);
        io.stamps.erase (kImage);

        AssertSucceeded (runner.OpenImage (kImage, opened, result),
            L"reading does not need the stamp and must still work");

        Assert::IsFalse (opened.stampRecorded);

        hr = runner.CommitImage (opened, EditedImageBytes(), result);

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
        DiskCommandRunner::OpenedImage opened;
        DiskCommandResult              result;
        std::string                    taken;
        vector<Byte>                   sentinel = { 'N', 'O', 'T', 'Y', 'O', 'U', 'R', 'S' };

        // Learn the name this runner reaches for, by watching it commit once.
        SeedRealDisk (io);
        AssertSucceeded (runner.OpenImage (kImage, opened, result));
        AssertSucceeded (runner.CommitImage (opened, EditedImageBytes(), result));

        taken = TemporaryPathChosen (io);

        // Put the image back, park something at that name, and make the SAME
        // runner commit again -- same runner, so the same name comes up first.
        SeedRealDisk (io);
        io.files[taken]  = sentinel;
        io.stamps[taken] = FileStamp { sentinel.size(), 1 };

        {
            DiskCommandRunner::OpenedImage second;
            DiskCommandResult              secondResult;

            AssertSucceeded (runner.OpenImage (kImage, second, secondResult));
            AssertSucceeded (runner.CommitImage (second, EditedImageBytes(), secondResult));
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
        DiskCommandRunner::OpenedImage openedA;
        DiskCommandRunner::OpenedImage openedB;
        DiskCommandResult              resultA;
        DiskCommandResult              resultB;

        SeedRealDisk (ioA);
        SeedRealDisk (ioB);

        AssertSucceeded (runnerA.OpenImage (kImage, openedA, resultA));
        AssertSucceeded (runnerB.OpenImage (kImage, openedB, resultB));
        AssertSucceeded (runnerA.CommitImage (openedA, EditedImageBytes(), resultA));
        AssertSucceeded (runnerB.CommitImage (openedB, EditedImageBytes(), resultB));

        Assert::IsFalse (TemporaryPathChosen (ioA) == TemporaryPathChosen (ioB),
            L"two invocations against one image must not reach for one name");
    }

    TEST_METHOD (InUseHelpText_SaysWhatTheProbeCannotSee)
    {
        // FR-035's actual demand is about the WORDING: the probe catches other
        // tools and cannot catch this emulator, so the help must not let a
        // reader conclude that a clean probe means their mounted disk is safe.
        // The claim lives beside the code that makes it, which is also what
        // lets this test read it -- the console executable is not linked here.
        std::string  text = DiskCommandRunner::kInUseHelpText;

        Assert::IsTrue (text.find ("another program holds the image open") != std::string::npos,
            L"it must say what the probe DOES catch");

        Assert::IsTrue (text.find ("cannot tell whether the image is mounted here")
                        != std::string::npos,
            L"and disclaim the one it cannot");

        Assert::IsTrue (text.find ("neither detected nor protected") != std::string::npos,
            L"in terms that leave no room for a reader to assume protection");
    }

    TEST_METHOD (Get_WithText_ToANamedFile_ConvertsThereToo)
    {
        // The conversion belongs to the payload, not to the destination. A
        // caller who redirects to a file and a caller who pipes must get the
        // same bytes.
        FakeDiskFileIo     io;
        DiskCommandRunner  runner (io);
        CommandLineOptions options = MakeOptions (CommandLineOptions::DiskOptions::Verb::Get);
        DiskCommandResult  piped;
        DiskCommandResult  written;

        SeedRealDisk (io);
        options.disk.path     = "T.SENDMSG";
        options.disk.encoding = CommandLineOptions::DiskOptions::Encoding::Text;

        piped = runner.Run (options);

        options.disk.hostFile = "C:\\out.txt";
        written               = runner.Run (options);

        Assert::AreEqual (DiskCommandRunner::kClean, written.exitStatus);
        Assert::AreEqual (size_t (1), io.files.count ("C:\\out.txt"));
        Assert::IsTrue (io.files["C:\\out.txt"] == piped.payload,
            L"the same conversion, whichever way the bytes leave");
    }
};
