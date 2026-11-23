#include "Pch.h"

#include "../EhmTestHelper.h"
#include "FakeDiskFileIo.h"
#include "Devices/Disk/DiskCommandRunner.h"
#include "DiskRunnerFixtures.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCommitTests
//
//  The commit path every editing command uses: the temporary file, the
//  replace, and the image left byte for byte as it was on each failure.
//
////////////////////////////////////////////////////////////////////////////////




TEST_CLASS (DiskCommitTests)
{
public:

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

    void AssertImageIsUntouched (FakeDiskFileIo & io)
    {
        vector<Byte>  original = DiskRunnerFixtures::OriginalImageBytes();
        size_t        i        = 0;

        Assert::AreEqual (size_t (1), io.files.count (DiskRunnerFixtures::kImage), L"the image must still be there");
        Assert::AreEqual (original.size(), io.files[DiskRunnerFixtures::kImage].size(),
            L"and be the size it was");

        for (i = 0; i < original.size(); i++)
        {
            if (io.files[DiskRunnerFixtures::kImage][i] != original[i])
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
        vector<Byte>  bytes = DiskRunnerFixtures::OriginalImageBytes();

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
            if (path != DiskRunnerFixtures::kImage)
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

        DiskRunnerFixtures::SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result));

        edited = EditedImageBytes();

        AssertSucceeded (runner.GetSession().CommitImage (opened, edited, result));

        Assert::IsTrue (io.files[DiskRunnerFixtures::kImage] == edited, L"the new bytes are the image now");
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

        DiskRunnerFixtures::SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result));
        AssertSucceeded (runner.GetSession().CommitImage (opened, EditedImageBytes(), result));

        for (const std::string & path : io.writtenPaths)
        {
            Assert::IsFalse (path == DiskRunnerFixtures::kImage,
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

        DiskRunnerFixtures::SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result));

        io.failNextWrite = true;

        hr = runner.GetSession().CommitImage (opened, EditedImageBytes(), result);

        Assert::IsTrue (FAILED (hr));
        AssertImageIsUntouched (io);
        Assert::AreEqual (1, io.removeCount, L"the partial temporary was swept");
        Assert::AreEqual (0, io.replaceCount, L"and nothing was ever put over the image");
        Assert::AreEqual (DiskCommandResult::kNoOutput, result.exitStatus);
        Assert::IsTrue (result.diagnostics.find (DiskRunnerFixtures::kImage) != std::string::npos,
            L"and the refusal identifies the image");
    }

    //  A name Windows rejects first fails at the temporary, and was once
    //  reported there as a read-only or full folder.
    TEST_METHOD (Commit_WhenTheNameIsInvalid_ReportsTheNameRatherThanTheFolder)
    {
        FakeDiskFileIo                 io;
        DiskCommandRunner              runner (io);
        DiskImageSession::OpenedImage  opened;
        DiskCommandResult              result;
        HRESULT                        hr     = S_OK;



        DiskRunnerFixtures::SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result));

        io.failNextWrite  = true;
        io.nextWriteError = HRESULT_FROM_WIN32 (ERROR_INVALID_NAME);

        hr = runner.GetSession().CommitImage (opened, EditedImageBytes(), result);

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_INVALID_NAME), hr);
        Assert::IsTrue (result.diagnostics.find (std::string (DiskRunnerFixtures::kImage) + ": is not a valid file name") != std::string::npos,
                        L"the refusal reports the name");
        Assert::IsTrue (result.diagnostics.find ("read-only") == std::string::npos,
                        L"and does not blame the folder");
    }

    TEST_METHOD (DescribeTemporaryWriteFailure_GivesTheCauseTheCodeReports)
    {
        struct Case
        {
            DWORD         win32Error;
            const char  * expected;
        };

        static constexpr Case  kCases[] =
        {
            { ERROR_INVALID_NAME,     "is not a valid file name"                          },
            { ERROR_PATH_NOT_FOUND,   "could not be written. Its folder does not exist"   },
            { ERROR_ACCESS_DENIED,    "could not be written. Its folder is read-only, or you do not have permission to write there" },
            { ERROR_DISK_FULL,        "could not be written. The disk is full"            },
            { ERROR_HANDLE_DISK_FULL, "could not be written. The disk is full"            },
            { ERROR_WRITE_FAULT,      "could not be written"                              },
        };



        for (const Case & c : kCases)
        {
            Assert::AreEqual (std::string (c.expected),
                              DiskImageSession::DescribeTemporaryWriteFailure (HRESULT_FROM_WIN32 (c.win32Error)));
        }

        Assert::AreEqual (std::string ("could not be written"),
                          DiskImageSession::DescribeTemporaryWriteFailure (E_FAIL),
                          L"a code with no specific cause gets no guessed one");
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

        DiskRunnerFixtures::SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result));

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

        DiskRunnerFixtures::SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result));

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

        DiskRunnerFixtures::SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result));

        io.stamps[DiskRunnerFixtures::kImage].sizeBytes += 1;

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

        DiskRunnerFixtures::SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result));

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

        DiskRunnerFixtures::SeedRealDisk (io);
        io.stamps.erase (DiskRunnerFixtures::kImage);

        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result),
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
        DiskRunnerFixtures::SeedRealDisk (io);
        AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, opened, result));
        AssertSucceeded (runner.GetSession().CommitImage (opened, EditedImageBytes(), result));

        taken = TemporaryPathChosen (io);

        // Put the image back, park something at that name, and make the SAME
        // runner commit again -- same runner, so the same name comes up first.
        DiskRunnerFixtures::SeedRealDisk (io);
        io.files[taken]  = sentinel;
        io.stamps[taken] = FileStamp { sentinel.size(), 1 };

        {
            DiskImageSession::OpenedImage  second;
            DiskCommandResult              secondResult;

            AssertSucceeded (runner.GetSession().OpenImage (DiskRunnerFixtures::kImage, second, secondResult));
            AssertSucceeded (runner.GetSession().CommitImage (second, EditedImageBytes(), secondResult));
        }

        Assert::AreEqual (size_t (1), io.files.count (taken),
            L"the file already there must survive");

        Assert::IsTrue (io.files[taken] == sentinel,
            L"byte for byte -- stepping over a name means not writing it");

        Assert::IsTrue (io.files[DiskRunnerFixtures::kImage] == EditedImageBytes(),
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

        DiskRunnerFixtures::SeedRealDisk (ioA);
        DiskRunnerFixtures::SeedRealDisk (ioB);

        AssertSucceeded (runnerA.GetSession().OpenImage (DiskRunnerFixtures::kImage, openedA, resultA));
        AssertSucceeded (runnerB.GetSession().OpenImage (DiskRunnerFixtures::kImage, openedB, resultB));
        AssertSucceeded (runnerA.GetSession().CommitImage (openedA, EditedImageBytes(), resultA));
        AssertSucceeded (runnerB.GetSession().CommitImage (openedB, EditedImageBytes(), resultB));

        Assert::IsFalse (TemporaryPathChosen (ioA) == TemporaryPathChosen (ioB),
            L"two invocations against one image must not reach for one name");
    }
};
