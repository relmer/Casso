#include "Pch.h"

#include "Devices/Disk/Store/DiskBayTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBayTableTests
//
//  Bay lookup and the queries across every bay. Each test sets the mounted
//  flag and path directly; no image is loaded and no file is touched.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskBayTableTests)
{
public:

    static void MarkMounted (DiskBayTable & table, int slot, int drive, const string & path)
    {
        DiskBay  & bay = table.GetBay (slot, drive);



        bay.mounted = true;
        bay.path    = path;
    }


    TEST_METHOD (EveryBay_KnowsItsOwnPosition)
    {
        DiskBayTable  table;
        int           slot  = 0;
        int           drive = 0;



        for (slot = 0; slot < DiskBayTable::kSlotCount; slot++)
        {
            for (drive = 0; drive < DiskBayTable::kDriveCount; drive++)
            {
                Assert::AreEqual (slot,  table.GetBay (slot, drive).slot);
                Assert::AreEqual (drive, table.GetBay (slot, drive).drive);
            }
        }
    }


    TEST_METHOD (IsValidBay_AcceptsOnlyTheFixedArray)
    {
        Assert::IsTrue  (DiskBayTable::IsValidBay (0, 0));
        Assert::IsTrue  (DiskBayTable::IsValidBay (DiskBayTable::kSlotCount - 1, DiskBayTable::kDriveCount - 1));
        Assert::IsFalse (DiskBayTable::IsValidBay (-1, 0));
        Assert::IsFalse (DiskBayTable::IsValidBay (0, -1));
        Assert::IsFalse (DiskBayTable::IsValidBay (DiskBayTable::kSlotCount, 0));
        Assert::IsFalse (DiskBayTable::IsValidBay (0, DiskBayTable::kDriveCount));
    }


    TEST_METHOD (IsFileInAnotherBay_FindsTheOtherDriveAndSkipsTheExcludedOne)
    {
        DiskBayTable  table;
        int           drive = -1;



        MarkMounted (table, 6, 1, "C:\\disks\\Work.dsk");

        Assert::IsTrue (table.IsFileInAnotherBay ("C:\\disks\\Work.dsk", 6, 0, drive));
        Assert::AreEqual (1, drive);

        Assert::IsFalse (table.IsFileInAnotherBay ("C:\\disks\\Work.dsk", 6, 1, drive),
            L"the bay being mounted into is not another bay");
        Assert::AreEqual (-1, drive);
    }


    TEST_METHOD (IsFileInAnotherBay_EmptyPathIsNeverAMatch)
    {
        DiskBayTable  table;
        int           drive = 0;



        MarkMounted (table, 5, 0, string());

        Assert::IsFalse (table.IsFileInAnotherBay (string(), 6, 0, drive));
    }


    TEST_METHOD (IsFileInAnotherBay_IgnoresAnUnmountedBay)
    {
        DiskBayTable  table;
        int           drive = 0;



        table.GetBay (6, 1).path = "C:\\disks\\Work.dsk";

        Assert::IsFalse (table.IsFileInAnotherBay ("C:\\disks\\Work.dsk", 6, 0, drive));
    }


    TEST_METHOD (IsDirectoryInAnotherBay_SeesASecondDiskFromTheSameFolder)
    {
        DiskBayTable  table;



        MarkMounted (table, 6, 0, "C:\\disks\\Boot.dsk");
        MarkMounted (table, 6, 1, "C:\\disks\\Work.dsk");

        Assert::IsTrue  (table.IsDirectoryInAnotherBay ("C:\\disks", 6, 0));
        Assert::IsFalse (table.IsDirectoryInAnotherBay ("C:\\other", 6, 0));
    }


    TEST_METHOD (IsDirectoryInAnotherBay_DoesNotCountTheExcludedBay)
    {
        DiskBayTable  table;



        MarkMounted (table, 6, 0, "C:\\disks\\Boot.dsk");

        Assert::IsFalse (table.IsDirectoryInAnotherBay ("C:\\disks", 6, 0));
    }


    TEST_METHOD (TakeNextMediaId_NeverRepeats)
    {
        DiskBayTable  table;
        uint64_t      first  = table.TakeNextMediaId();
        uint64_t      second = table.TakeNextMediaId();



        Assert::IsTrue (first != 0, L"0 is the id of a bay that never mounted");
        Assert::IsTrue (second > first);
    }
};
