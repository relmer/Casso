#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/StockDisksListing.h"
#include "Devices/Disk/DriveImageRule.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StockDisksListingTests
//
//  The rows of a picker's "Stock disks" node.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (StockDisksListingTests)
{
public:

    TEST_METHOD (EachStockDiskIsARow_WithItsPathAndWhetherItIsDownloaded)
    {
        std::vector<StockDiskRow>  disks = { { L"DOS 3.3 System Master", L"C:\\Stock\\DOS 3.3 System Master.dsk", true },
                                             { L"ProDOS Users Disk",     L"C:\\Stock\\ProDOS Users Disk.dsk",     false } };
        std::vector<CatalogRow>    rows;



        StockDisksListing::Build (disks, rows);

        Assert::AreEqual ((size_t) 2, rows.size());
        Assert::AreEqual (std::wstring (L"DOS 3.3 System Master"), rows[0].name);
        Assert::AreEqual (std::wstring (L"C:\\Stock\\DOS 3.3 System Master.dsk"), rows[0].hostPath);
        Assert::IsTrue  (rows[0].isStockDisk);
        Assert::IsTrue  (rows[0].stockDownloaded);
        Assert::IsTrue  (rows[1].isStockDisk);
        Assert::IsFalse (rows[1].stockDownloaded);
        Assert::IsFalse (rows[0].isDirectory);
    }



    //  A stock disk is a 140K 5.25-inch disk before its file is on the disk,
    //  so the drive rule applies to its row.
    TEST_METHOD (StockRowHasTheClassOfA140KDisk)
    {
        std::vector<CatalogRow>  rows;



        StockDisksListing::Build ({ { L"ProDOS Users Disk", L"C:\\Stock\\ProDOS Users Disk.dsk", false } }, rows);

        Assert::IsTrue  (DriveImageRule::Accepts (ReceivingDrive { ReceivingDriveKind::Floppy525, 0 }, rows[0].imageClass));
        Assert::IsFalse (DriveImageRule::Accepts (ReceivingDrive { ReceivingDriveKind::Apple35, 0 }, rows[0].imageClass));
    }



    TEST_METHOD (NoStockDisks_NoRows)
    {
        std::vector<CatalogRow>  rows = { CatalogRow() };



        StockDisksListing::Build ({}, rows);

        Assert::IsTrue (rows.empty());
    }
};
