#include "Pch.h"

#include "../EhmTestHelper.h"
#include "CassoExplorer/Browser/BrowserCells.h"
#include "CassoExplorer/Model/StockDisksListing.h"
#include "CassoExplorer/Picker/StartupDiskPicker.h"
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
        std::vector<StockDiskRow>  disks = { { L"DOS 3.3 System Master", L"C:\\Stock\\DOS 3.3 System Master.dsk", true, L"Boots on every Apple II with a Disk II drive." },
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
        Assert::AreEqual (std::wstring (L"Boots on every Apple II with a Disk II drive."), rows[0].stockNote, L"the tooltip's text");
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



    //  A stock disk that isn't downloaded has no file, so its Size cell is
    //  blank; a downloaded one has its file's size.
    TEST_METHOD (StockRowSize_IsBlankUntilDownloaded)
    {
        std::vector<CatalogRow>  rows;
        std::wstring             blank;
        std::wstring             downloaded;



        StockDisksListing::Build ({ { L"ProDOS Users Disk", L"C:\\Stock\\ProDOS Users Disk.dsk", false },
                                    { L"DOS 3.3 System Master", L"C:\\Stock\\DOS 3.3 System Master.dsk", true } }, rows);

        for (const DxuiListView::Cell & cell : BrowserCells::ToCells (rows[0]))
        {
            blank += cell.text + L"|";
        }

        for (const DxuiListView::Cell & cell : BrowserCells::ToCells (rows[1]))
        {
            downloaded += cell.text + L"|";
        }

        Assert::AreEqual (std::wstring::npos, blank.find (BrowserCells::FormatSizeColumn (0)), blank.c_str());
        Assert::AreNotEqual (std::wstring::npos, downloaded.find (BrowserCells::FormatSizeColumn (rows[1].sizeBytes)), downloaded.c_str());
    }



    //  The stock disks are the DOS 3.3 System Master, Apple's ProDOS 1.0.1
    //  Users Disk and ProDOS 2.4.3, all 140K, so a 3.5-inch drive's list is empty.
    TEST_METHOD (StockDisks_AreTheMastersAndProDos243_ForA525Drive)
    {
        std::vector<StockDiskRow>  floppy  = StartupDiskPicker::GetStockDisks (ReceivingDrive { ReceivingDriveKind::Floppy525, 0 });
        std::vector<StockDiskRow>  apple35 = StartupDiskPicker::GetStockDisks (ReceivingDrive { ReceivingDriveKind::Apple35, 0 });



        Assert::AreEqual ((size_t) 3, floppy.size());
        Assert::AreEqual (std::wstring (L"DOS 3.3 System Master"),   floppy[0].label);
        Assert::AreEqual (std::wstring (L"ProDOS 1.0.1 Users Disk"), floppy[1].label);
        Assert::AreEqual (std::wstring (L"ProDOS 2.4.3"),            floppy[2].label);
        Assert::IsTrue   (floppy[1].note.find (L"64K") != std::wstring::npos, L"ProDOS needs 64K");
        Assert::IsTrue   (floppy[2].path.ends_with (L"ProDOS 2.4.3.po"));
        Assert::IsTrue   (apple35.empty());
    }



    TEST_METHOD (NoStockDisks_NoRows)
    {
        std::vector<CatalogRow>  rows = { CatalogRow() };



        StockDisksListing::Build ({}, rows);

        Assert::IsTrue (rows.empty());
    }
};
