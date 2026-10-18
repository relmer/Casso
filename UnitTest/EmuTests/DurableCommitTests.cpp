#include "Pch.h"

#include "../EhmTestHelper.h"
#include "Devices/Disk/DurableCommit.h"
#include "FakeDiskFileIo.h"
#include "HResultAssert.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DurableCommitTests
//
//  The commit sequence over the in-memory file table: Replace and CreateNew
//  each succeed in order (write, metadata, flush, then replace or rename),
//  and a failure at any step leaves the target byte for byte as it was and
//  no temporary behind.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DurableCommitTests)
{
public:

    static constexpr const char *  kTarget = "C:\\disks\\image.dsk";
    static constexpr uint64_t      kTag    = 0x1234;



    static vector<Byte> MakeBytes (Byte fill)
    {
        return vector<Byte> (512, fill);
    }



    static void Seed (FakeDiskFileIo & io)
    {
        io.files[kTarget]  = MakeBytes (0x11);
        io.stamps[kTarget] = FileStamp { 512, 1 };
    }



    TEST_METHOD (ReplaceWritesCopiesFlushesThenReplaces)
    {
        FakeDiskFileIo        io;
        CommitPlan::Progress  progress;



        Seed (io);

        AssertSucceeded (DurableCommit::Commit (io, kTarget, MakeBytes (0x22), kTag, CommitMode::Replace, progress));

        Assert::IsTrue (io.files[kTarget] == MakeBytes (0x22));
        Assert::AreEqual (1, io.metadataCount, L"the old file's metadata goes to the new one");
        Assert::AreEqual (1, io.flushCount);
        Assert::AreEqual (io.writtenPaths[0], io.flushedPaths[0], L"the temporary is flushed, not the target");
        Assert::AreEqual (1, io.replaceCount);
        Assert::AreEqual (0, io.renameCount);
        Assert::IsTrue (progress.replaceSucceeded);
        Assert::IsTrue (io.HasNoTemporaryFiles());
    }



    TEST_METHOD (ReplaceOfAMissingFileCopiesNoMetadata)
    {
        FakeDiskFileIo        io;
        CommitPlan::Progress  progress;



        AssertSucceeded (DurableCommit::Commit (io, kTarget, MakeBytes (0x22), kTag, CommitMode::Replace, progress));

        Assert::AreEqual (0, io.metadataCount);
        Assert::IsTrue (io.files[kTarget] == MakeBytes (0x22));
    }



    TEST_METHOD (CreateNewRenamesAndNeverOverwrites)
    {
        FakeDiskFileIo        io;
        CommitPlan::Progress  progress;
        CommitPlan::Progress  again;



        AssertSucceeded (DurableCommit::Commit (io, kTarget, MakeBytes (0x33), kTag, CommitMode::CreateNew, progress));

        Assert::AreEqual (1, io.renameCount);
        Assert::AreEqual (0, io.replaceCount);
        Assert::AreEqual (0, io.metadataCount, L"a new file has no old metadata to keep");
        Assert::IsTrue (io.files[kTarget] == MakeBytes (0x33));

        Assert::AreEqual (HRESULT_FROM_WIN32 (ERROR_ALREADY_EXISTS), DurableCommit::Commit (io, kTarget, MakeBytes (0x44), kTag, CommitMode::CreateNew, again));
        Assert::IsTrue (io.files[kTarget] == MakeBytes (0x33), L"the existing file is untouched");
        Assert::IsTrue (io.HasNoTemporaryFiles());
    }



    TEST_METHOD (AFailureAtAnyStepLeavesTheTargetAndNoTemporary)
    {
        struct Case
        {
            const wchar_t *         name;
            CommitMode              mode;
            CommitPlan::Step        step;
            void                  (*arm) (FakeDiskFileIo & io);
        };

        const Case  cases[] =
        {
            { L"write",    CommitMode::Replace,   CommitPlan::Step::WriteTemporary, [] (FakeDiskFileIo & io) { io.failNextWrite    = true; } },
            { L"metadata", CommitMode::Replace,   CommitPlan::Step::CopyMetadata,   [] (FakeDiskFileIo & io) { io.failNextMetadata = true; } },
            { L"flush",    CommitMode::Replace,   CommitPlan::Step::FlushTemporary, [] (FakeDiskFileIo & io) { io.failNextFlush    = true; } },
            { L"replace",  CommitMode::Replace,   CommitPlan::Step::Replace,        [] (FakeDiskFileIo & io) { io.failNextReplace  = true; } },
            { L"rename",   CommitMode::CreateNew, CommitPlan::Step::Replace,        [] (FakeDiskFileIo & io) { io.failNextRename   = true; } },
        };

        for (const Case & c : cases)
        {
            FakeDiskFileIo        io;
            CommitPlan::Progress  progress;
            HRESULT               hr       = S_OK;

            if (c.mode == CommitMode::Replace)
            {
                Seed (io);
            }

            c.arm (io);
            hr = DurableCommit::Commit (io, kTarget, MakeBytes (0x55), kTag, c.mode, progress);

            Assert::IsTrue (FAILED (hr), c.name);
            Assert::IsTrue (progress.furthestAttempted == c.step, c.name);
            Assert::IsFalse (progress.replaceSucceeded, c.name);
            Assert::IsTrue (io.HasNoTemporaryFiles(), c.name);

            if (c.mode == CommitMode::Replace)
            {
                Assert::IsTrue (io.files[kTarget] == MakeBytes (0x11), c.name);
            }
            else
            {
                Assert::IsFalse (io.Exists (kTarget), c.name);
            }
        }
    }



    TEST_METHOD (AnAbandonedTemporaryIsSteppedOver)
    {
        FakeDiskFileIo        io;
        CommitPlan::Progress  progress;
        std::string           abandoned = CommitPlan::GetTemporaryPath (kTarget, kTag, 0);



        Seed (io);
        io.files[abandoned] = MakeBytes (0x99);

        AssertSucceeded (DurableCommit::Commit (io, kTarget, MakeBytes (0x22), kTag, CommitMode::Replace, progress));

        Assert::AreNotEqual (abandoned, io.writtenPaths[0]);
        Assert::IsTrue (io.files[abandoned] == MakeBytes (0x99), L"another commit's temporary is not touched");
    }
};
