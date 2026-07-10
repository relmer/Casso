#include "Pch.h"

#include "../Casso/Shell/DiskMru.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace DiskMruTests
{
    TEST_CLASS (DiskMruTests)
    {
    public:

        TEST_METHOD (Insert_IntoEmpty_Adds)
        {
            DiskMru  m;
            m.RecordMount (L"C:\\Disks\\A.dsk");

            Assert::AreEqual ((size_t) 1, m.Size());
        }


        TEST_METHOD (Remount_MovesToFront_NoDuplicate)
        {
            DiskMru  m;
            m.RecordMount (L"C:\\Disks\\A.dsk");
            m.RecordMount (L"C:\\Disks\\B.dsk");
            m.RecordMount (L"C:\\Disks\\A.dsk");

            auto  snap = m.Snapshot();
            Assert::AreEqual ((size_t) 2, snap.size());
            Assert::IsTrue (snap[0].path == std::filesystem::path (L"C:\\Disks\\A.dsk"));
            Assert::IsTrue (snap[1].path == std::filesystem::path (L"C:\\Disks\\B.dsk"));
        }


        TEST_METHOD (Eviction_AtCapacity_DropsOldest)
        {
            DiskMru  m;
            size_t   i = 0;

            for (i = 0; i < DiskMru::k_capacity + 1; i++)
            {
                std::wstring  p = L"C:\\Disks\\";
                p += std::to_wstring (i);
                p += L".dsk";
                m.RecordMount (p);
            }

            Assert::AreEqual (DiskMru::k_capacity, m.Size());

            // The most-recent (last inserted, index k_capacity) is at front;
            // the original index-0 entry should have been evicted.
            auto  snap = m.Snapshot();
            Assert::IsTrue (snap[0].path.wstring().find (L"\\16.dsk") != std::wstring::npos);
            for (i = 0; i < snap.size(); i++)
            {
                Assert::IsTrue (snap[i].path.wstring().find (L"\\0.dsk") == std::wstring::npos);
            }
        }


        TEST_METHOD (Prune_DropsRejectedEntries_PreservesOrder)
        {
            DiskMru  m;
            m.RecordMount (L"C:\\Disks\\A.dsk");
            m.RecordMount (L"C:\\Disks\\B.dsk");
            m.RecordMount (L"C:\\Disks\\C.dsk");

            auto  result = m.Prune ([] (const std::filesystem::path & p) {
                return p.wstring().find (L"B.dsk") == std::wstring::npos;
            });

            Assert::AreEqual ((size_t) 2, result.size());
            Assert::IsTrue (result[0].path == std::filesystem::path (L"C:\\Disks\\C.dsk"));
            Assert::IsTrue (result[1].path == std::filesystem::path (L"C:\\Disks\\A.dsk"));
        }


        TEST_METHOD (Prune_NullPredicate_NoOp)
        {
            DiskMru  m;
            m.RecordMount (L"C:\\Disks\\A.dsk");

            auto  result = m.Prune (nullptr);
            Assert::AreEqual ((size_t) 1, result.size());
        }


        TEST_METHOD (Prune_Idempotent)
        {
            DiskMru  m;
            m.RecordMount (L"C:\\Disks\\A.dsk");
            m.RecordMount (L"C:\\Disks\\B.dsk");

            auto  predicate = [] (const std::filesystem::path & p) {
                return p.wstring().find (L"A") != std::wstring::npos;
            };

            auto  first  = m.Prune (predicate);
            auto  second = m.Prune (predicate);

            Assert::AreEqual (first.size(), second.size());
            Assert::IsTrue (first == second);
        }


        TEST_METHOD (Snapshot_MostRecentFirst)
        {
            DiskMru  m;
            m.RecordMount (L"C:\\Disks\\A.dsk");
            m.RecordMount (L"C:\\Disks\\B.dsk");

            auto  snap = m.Snapshot();
            Assert::IsTrue (snap[0].path == std::filesystem::path (L"C:\\Disks\\B.dsk"));
            Assert::IsTrue (snap[1].path == std::filesystem::path (L"C:\\Disks\\A.dsk"));
        }


        TEST_METHOD (Empty_RecordMount_OnEmptyPathIgnored)
        {
            DiskMru  m;
            m.RecordMount (std::filesystem::path {});

            Assert::IsTrue (m.Empty());
        }


        TEST_METHOD (ReplaceAll_EnforcesCap)
        {
            DiskMru                          m;
            std::vector<DiskMru::Entry>      many;
            size_t                           i = 0;

            for (i = 0; i < DiskMru::k_capacity + 5; i++)
            {
                std::wstring  p = L"C:\\X\\";
                p += std::to_wstring (i);
                many.push_back (DiskMru::Entry { std::filesystem::path (p), 0 });
            }

            m.ReplaceAll (many);
            Assert::AreEqual (DiskMru::k_capacity, m.Size());
        }


        TEST_METHOD (FromUtf8_DropsEmpty_PreservesOrder)
        {
            std::vector<std::string>  in = { "C:\\Disks\\A.dsk", "", "C:\\Disks\\B.dsk" };
            auto  mru = DiskMru::FromUtf8 (in);
            auto  snap = mru.Snapshot();
            Assert::AreEqual ((size_t) 2, snap.size());
            Assert::IsTrue (snap[0].path == std::filesystem::path ("C:\\Disks\\A.dsk"));
            Assert::IsTrue (snap[1].path == std::filesystem::path ("C:\\Disks\\B.dsk"));
        }


        TEST_METHOD (ToUtf8_RoundTrips)
        {
            DiskMru  mru;
            mru.RecordMount (L"C:\\Disks\\A.dsk");
            mru.RecordMount (L"C:\\Disks\\B.dsk");

            std::vector<std::string>   out;
            std::vector<std::int64_t>  times;
            mru.ToUtf8 (out, times);

            Assert::AreEqual ((size_t) 2, out.size());
            Assert::AreEqual (std::string ("C:\\Disks\\B.dsk"), out[0]);
            Assert::AreEqual (std::string ("C:\\Disks\\A.dsk"), out[1]);
        }


        TEST_METHOD (Utf8RoundTrip_PreservesNonAsciiFilename)
        {
            // Regression: a non-ASCII filename (the o-slash in "Broderbund")
            // must survive the wide -> UTF-8 -> wide round-trip intact. The
            // old platform-narrow conversion mangled it into invalid UTF-8,
            // so the boot picker's exists-prune silently dropped the entry.
            const wchar_t *  kName = L"C:\\Disks\\Space Quarks (Br\u00F8derbund).woz";

            DiskMru  mru;
            mru.RecordMount (kName);

            std::vector<std::string>   serialized;
            std::vector<std::int64_t>  times;
            mru.ToUtf8 (serialized, times);

            Assert::AreEqual ((size_t) 1, serialized.size());

            // The o-slash (U+00F8) must encode as the two UTF-8 bytes 0xC3 0xB8.
            Assert::IsTrue (serialized[0].find ("\xC3\xB8") != std::string::npos,
                L"Serialized path must contain valid UTF-8 for U+00F8");

            DiskMru  reloaded = DiskMru::FromUtf8 (serialized);
            auto     snap     = reloaded.Snapshot();

            Assert::AreEqual ((size_t) 1, snap.size());
            Assert::IsTrue (snap[0].path == std::filesystem::path (kName),
                L"Reloaded path must equal the original after round-trip");
        }


        TEST_METHOD (RecordMount_StampsLoadTime)
        {
            constexpr std::int64_t  kWhen = 1700000000;

            DiskMru  m;
            m.RecordMount (L"C:\\Disks\\A.dsk", kWhen);

            auto  snap = m.Snapshot();
            Assert::AreEqual ((size_t) 1, snap.size());
            Assert::AreEqual (kWhen, snap[0].lastLoadedUnix);
        }


        TEST_METHOD (Remount_RefreshesLoadTime)
        {
            constexpr std::int64_t  kFirst  = 1700000000;
            constexpr std::int64_t  kSecond = 1700009999;

            DiskMru  m;
            m.RecordMount (L"C:\\Disks\\A.dsk", kFirst);
            m.RecordMount (L"C:\\Disks\\A.dsk", kSecond);

            auto  snap = m.Snapshot();
            Assert::AreEqual ((size_t) 1, snap.size());
            Assert::AreEqual (kSecond, snap[0].lastLoadedUnix);
        }


        TEST_METHOD (RecordMount_DefaultLoadTimeIsUnknownZero)
        {
            DiskMru  m;
            m.RecordMount (L"C:\\Disks\\A.dsk");

            auto  snap = m.Snapshot();
            Assert::AreEqual ((std::int64_t) 0, snap[0].lastLoadedUnix);
        }


        TEST_METHOD (ToUtf8_FromUtf8_RoundTripsLoadTimes)
        {
            constexpr std::int64_t  kWhenA = 1700000001;
            constexpr std::int64_t  kWhenB = 1700000002;

            DiskMru  mru;
            mru.RecordMount (L"C:\\Disks\\A.dsk", kWhenA);
            mru.RecordMount (L"C:\\Disks\\B.dsk", kWhenB);

            std::vector<std::string>   paths;
            std::vector<std::int64_t>  times;
            mru.ToUtf8 (paths, times);

            Assert::AreEqual ((size_t) 2, times.size());

            DiskMru  reloaded = DiskMru::FromUtf8 (paths, times);
            auto     snap     = reloaded.Snapshot();

            // Most-recent-first: B (kWhenB) then A (kWhenA).
            Assert::AreEqual (kWhenB, snap[0].lastLoadedUnix);
            Assert::AreEqual (kWhenA, snap[1].lastLoadedUnix);
        }


        TEST_METHOD (FromUtf8_ShorterTimesArray_DefaultsMissingToZero)
        {
            // Legacy prefs shape: paths present, no (or partial) load times.
            std::vector<std::string>   paths = { "C:\\Disks\\A.dsk", "C:\\Disks\\B.dsk" };
            std::vector<std::int64_t>  times = { 1700000000 };   // only the first entry has a time

            auto  mru  = DiskMru::FromUtf8 (paths, times);
            auto  snap = mru.Snapshot();

            Assert::AreEqual ((size_t) 2, snap.size());
            Assert::AreEqual ((std::int64_t) 1700000000, snap[0].lastLoadedUnix);
            Assert::AreEqual ((std::int64_t) 0,          snap[1].lastLoadedUnix);
        }


        //////////////////////////////////////////////////////////////////////
        //
        //  DistinctFolders
        //
        //  The disk picker scans these folders for sibling images, so the
        //  set must be de-duped (recents in one folder collapse to that one
        //  directory) yet keep first-seen order, be case-insensitive (the
        //  host filesystem is), and skip entries with no folder.
        //
        //////////////////////////////////////////////////////////////////////

        static std::vector<DiskMru::Entry> MakeEntries (std::initializer_list<const wchar_t *> paths)
        {
            std::vector<DiskMru::Entry>  entries;

            for (const wchar_t * p : paths)
            {
                entries.push_back (DiskMru::Entry { std::filesystem::path (p), 0 });
            }
            return entries;
        }


        TEST_METHOD (DistinctFolders_Empty_ReturnsEmpty)
        {
            auto  folders = DiskMru::DistinctFolders ({});

            Assert::AreEqual ((size_t) 0, folders.size());
        }


        TEST_METHOD (DistinctFolders_SameFolder_CollapsesToOne)
        {
            auto  entries = MakeEntries ({ L"C:\\Disks\\A.dsk",
                                           L"C:\\Disks\\B.woz",
                                           L"C:\\Disks\\C.po" });
            auto  folders = DiskMru::DistinctFolders (entries);

            Assert::AreEqual ((size_t) 1, folders.size());
            Assert::IsTrue (folders[0] == std::filesystem::path (L"C:\\Disks"));
        }


        TEST_METHOD (DistinctFolders_DifferentFolders_PreserveFirstSeenOrder)
        {
            auto  entries = MakeEntries ({ L"C:\\Two\\B.dsk",
                                           L"C:\\One\\A.dsk",
                                           L"C:\\Two\\C.dsk",     // repeat of \Two
                                           L"C:\\Three\\D.dsk" });
            auto  folders = DiskMru::DistinctFolders (entries);

            Assert::AreEqual ((size_t) 3, folders.size());
            Assert::IsTrue (folders[0] == std::filesystem::path (L"C:\\Two"));
            Assert::IsTrue (folders[1] == std::filesystem::path (L"C:\\One"));
            Assert::IsTrue (folders[2] == std::filesystem::path (L"C:\\Three"));
        }


        TEST_METHOD (DistinctFolders_CaseInsensitive_CollapsesFoldedDuplicate)
        {
            // The host filesystem is case-insensitive, so two recents whose
            // folders differ only by case are the SAME directory and must be
            // scanned once. The first-seen spelling is the one returned.
            auto  entries = MakeEntries ({ L"C:\\Disks\\A.dsk",
                                           L"C:\\DISKS\\B.dsk" });
            auto  folders = DiskMru::DistinctFolders (entries);

            Assert::AreEqual ((size_t) 1, folders.size());
            Assert::IsTrue (folders[0] == std::filesystem::path (L"C:\\Disks"));
        }


        TEST_METHOD (DistinctFolders_BareFilename_Skipped)
        {
            // An entry with no parent directory (a bare filename) has no
            // folder to scan and must be dropped rather than yielding an
            // empty path we would then try to enumerate.
            auto  entries = MakeEntries ({ L"loose.dsk",
                                           L"C:\\Disks\\A.dsk" });
            auto  folders = DiskMru::DistinctFolders (entries);

            Assert::AreEqual ((size_t) 1, folders.size());
            Assert::IsTrue (folders[0] == std::filesystem::path (L"C:\\Disks"));
        }
    };
}
