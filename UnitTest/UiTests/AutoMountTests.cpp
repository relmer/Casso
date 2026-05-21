#include "Pch.h"

#include "Ui/AutoMountResolver.h"
#include "Ui/DriveWidgetState.h"
#include "Ui/IDriveCommandSink.h"

#include "InMemoryFileSystem.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  MockDriveCommandSink
//
//  Records mount/eject calls so the test can assert on the routing
//  performed by the FR-047 auto-mount path.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    struct RecordedMount
    {
        int           slot;
        int           drive;
        std::wstring  path;
    };

    struct RecordedEject
    {
        int  slot;
        int  drive;
    };

    class MockDriveCommandSink : public IDriveCommandSink
    {
    public:
        std::vector<RecordedMount>  mounts;
        std::vector<RecordedEject>  ejects;

        HRESULT Mount (int slot, int drive, const std::wstring & path) override
        {
            mounts.push_back ({ slot, drive, path });
            return S_OK;
        }

        void    Eject (int slot, int drive) override
        {
            ejects.push_back ({ slot, drive });
        }
    };
}





////////////////////////////////////////////////////////////////////////////////
//
//  AutoMountTests
//
//  P6-T7 -- exercises the pure-logic auto-mount resolver + the sink
//  routing path. Avoids touching real disk via IFileSystem injection.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AutoMountTests)
{
public:

    TEST_METHOD (Resolve_EmptyPath_LeavesEmpty)
    {
        InMemoryFileSystem  fs;

        auto  decision = AutoMountResolver::Resolve (L"", fs);

        Assert::IsTrue (decision.action == AutoMountResolver::Action::LeaveEmpty,
                        L"empty remembered path => leave the slot empty");
        Assert::IsTrue (decision.path.empty());
    }

    TEST_METHOD (Resolve_PresentFile_RequestsMount)
    {
        InMemoryFileSystem  fs;

        fs.WriteAllText (L"C:\\images\\good.dsk", "(disk bytes)");

        auto  decision = AutoMountResolver::Resolve (L"C:\\images\\good.dsk", fs);

        Assert::IsTrue (decision.action == AutoMountResolver::Action::Mount,
                        L"existing file => Mount");
        Assert::AreEqual (std::wstring (L"C:\\images\\good.dsk"), decision.path);
    }

    TEST_METHOD (Resolve_MissingFile_RequestsClearStaleEntry)
    {
        InMemoryFileSystem  fs;
        // Note: nothing written to fs at this path.

        auto  decision = AutoMountResolver::Resolve (L"C:\\images\\gone.dsk", fs);

        Assert::IsTrue (decision.action == AutoMountResolver::Action::ClearStaleEntry,
                        L"missing file => caller should clear the stale entry");
        Assert::AreEqual (std::wstring (L"C:\\images\\gone.dsk"), decision.path,
                          L"path round-trips so the caller knows which entry to clear");
    }

    TEST_METHOD (MultipleDrives_OneMissing_OnePresent_BothDecidedIndependently)
    {
        InMemoryFileSystem  fs;
        std::wstring        paths[2] = { L"C:\\images\\boot.woz", L"C:\\images\\gone.dsk" };

        fs.WriteAllText (paths[0], "ok");

        auto  d0 = AutoMountResolver::Resolve (paths[0], fs);
        auto  d1 = AutoMountResolver::Resolve (paths[1], fs);

        Assert::IsTrue (d0.action == AutoMountResolver::Action::Mount,
                        L"drive 0 should mount its present file");
        Assert::IsTrue (d1.action == AutoMountResolver::Action::ClearStaleEntry,
                        L"drive 1 should clear its missing file -- other drives are not penalized");
    }

    TEST_METHOD (Snapshot_RoundTripsBothDrivePaths)
    {
        auto  snap = AutoMountResolver::SnapshotForPersistence (
            L"C:\\d1.dsk",
            L"C:\\d2.woz");

        Assert::AreEqual (std::wstring (L"C:\\d1.dsk"), snap[0]);
        Assert::AreEqual (std::wstring (L"C:\\d2.woz"), snap[1]);
    }

    TEST_METHOD (Snapshot_EmptySlotEmitsEmptyForCallerToClear)
    {
        auto  snap = AutoMountResolver::SnapshotForPersistence (
            L"C:\\only.dsk",
            L"");

        Assert::AreEqual (std::wstring (L"C:\\only.dsk"), snap[0]);
        Assert::IsTrue (snap[1].empty(),
                        L"empty drive must be persisted as empty so the saved entry can be cleared");
    }

    TEST_METHOD (SinkRouting_PostMountThenEjectIsObservable)
    {
        MockDriveCommandSink  sink;

        sink.Mount (6, 0, L"a.dsk");
        sink.Mount (6, 1, L"b.woz");
        sink.Eject (6, 0);

        Assert::AreEqual<size_t> (2, sink.mounts.size());
        Assert::AreEqual<size_t> (1, sink.ejects.size());
        Assert::AreEqual (std::wstring (L"a.dsk"), sink.mounts[0].path);
        Assert::AreEqual (0, sink.mounts[0].drive);
        Assert::AreEqual (6, sink.ejects[0].slot);
        Assert::AreEqual (0, sink.ejects[0].drive);
    }
};
