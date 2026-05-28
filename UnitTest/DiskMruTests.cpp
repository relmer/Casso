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
    };
}
