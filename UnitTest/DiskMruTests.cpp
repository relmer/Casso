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
            Assert::IsTrue (snap[0] == std::filesystem::path (L"C:\\Disks\\A.dsk"));
            Assert::IsTrue (snap[1] == std::filesystem::path (L"C:\\Disks\\B.dsk"));
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
            Assert::IsTrue (snap[0].wstring().find (L"\\16.dsk") != std::wstring::npos);
            for (i = 0; i < snap.size(); i++)
            {
                Assert::IsTrue (snap[i].wstring().find (L"\\0.dsk") == std::wstring::npos);
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
            Assert::IsTrue (result[0] == std::filesystem::path (L"C:\\Disks\\C.dsk"));
            Assert::IsTrue (result[1] == std::filesystem::path (L"C:\\Disks\\A.dsk"));
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
            Assert::IsTrue (snap[0] == std::filesystem::path (L"C:\\Disks\\B.dsk"));
            Assert::IsTrue (snap[1] == std::filesystem::path (L"C:\\Disks\\A.dsk"));
        }


        TEST_METHOD (Empty_RecordMount_OnEmptyPathIgnored)
        {
            DiskMru  m;
            m.RecordMount (std::filesystem::path {});

            Assert::IsTrue (m.Empty());
        }


        TEST_METHOD (ReplaceAll_EnforcesCap)
        {
            DiskMru                             m;
            std::vector<std::filesystem::path>  many;
            size_t                              i = 0;

            for (i = 0; i < DiskMru::k_capacity + 5; i++)
            {
                std::wstring  p = L"C:\\X\\";
                p += std::to_wstring (i);
                many.emplace_back (p);
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
            Assert::IsTrue (snap[0] == std::filesystem::path ("C:\\Disks\\A.dsk"));
            Assert::IsTrue (snap[1] == std::filesystem::path ("C:\\Disks\\B.dsk"));
        }


        TEST_METHOD (ToUtf8_RoundTrips)
        {
            DiskMru  mru;
            mru.RecordMount (L"C:\\Disks\\A.dsk");
            mru.RecordMount (L"C:\\Disks\\B.dsk");

            std::vector<std::string>  out;
            mru.ToUtf8 (out);

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

            std::vector<std::string>  serialized;
            mru.ToUtf8 (serialized);

            Assert::AreEqual ((size_t) 1, serialized.size());

            // The o-slash (U+00F8) must encode as the two UTF-8 bytes 0xC3 0xB8.
            Assert::IsTrue (serialized[0].find ("\xC3\xB8") != std::string::npos,
                L"Serialized path must contain valid UTF-8 for U+00F8");

            DiskMru  reloaded = DiskMru::FromUtf8 (serialized);
            auto     snap     = reloaded.Snapshot();

            Assert::AreEqual ((size_t) 1, snap.size());
            Assert::IsTrue (snap[0] == std::filesystem::path (kName),
                L"Reloaded path must equal the original after round-trip");
        }
    };
}
