#include "Pch.h"

#include "Devices/Printer/PrintPagination.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/PrinterTypes.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Pagination for the Windows-printer destination (FR-014): the fanfold strip is
// sliced into physical pages capped at PrinterGrid::kPageRows, with guest form
// feeds as hard early breaks. PNG / clipboard never paginate.
namespace PrintPaginationTests
{
    static PrintRaster Make (int rows, const vector<int> & boundaries = {})
    {
        PrintRaster   r;

        // Empty cells -> zero-filled strip; only rowsUsed + boundaries matter.
        r.RestoreFromIndexed (rows, {}, rows, boundaries, false);
        return r;
    }


    TEST_CLASS (PrintPaginationTests)
    {
    public:

        TEST_METHOD (EmptyStripHasNoPages)
        {
            Assert::AreEqual ((size_t) 0, PrintPagination::Paginate (Make (0)).size());
        }


        TEST_METHOD (ShortStripIsOnePage)
        {
            auto   pages = PrintPagination::Paginate (Make (500));

            Assert::AreEqual ((size_t) 1, pages.size());
            Assert::AreEqual (0,   pages[0].firstRow);
            Assert::AreEqual (499, pages[0].lastRow);
        }


        TEST_METHOD (ExactlyOnePhysicalPage)
        {
            auto   pages = PrintPagination::Paginate (Make (PrinterGrid::kPageRows));

            Assert::AreEqual ((size_t) 1, pages.size());
            Assert::AreEqual (0,                            pages[0].firstRow);
            Assert::AreEqual (PrinterGrid::kPageRows - 1,   pages[0].lastRow);
        }


        TEST_METHOD (OneRowOverSpillsToSecondPage)
        {
            auto   pages = PrintPagination::Paginate (Make (PrinterGrid::kPageRows + 1));

            Assert::AreEqual ((size_t) 2, pages.size());
            Assert::AreEqual (PrinterGrid::kPageRows, pages[1].firstRow);
            Assert::AreEqual (PrinterGrid::kPageRows, pages[1].lastRow);
        }


        TEST_METHOD (LongBannerSlicesByPageHeight)
        {
            int    rows  = PrinterGrid::kPageRows * 2 + 1044;   // three pages
            auto   pages = PrintPagination::Paginate (Make (rows));

            Assert::AreEqual ((size_t) 3, pages.size());
            Assert::AreEqual (0,                              pages[0].firstRow);
            Assert::AreEqual (PrinterGrid::kPageRows - 1,     pages[0].lastRow);
            Assert::AreEqual (PrinterGrid::kPageRows,         pages[1].firstRow);
            Assert::AreEqual (2 * PrinterGrid::kPageRows - 1, pages[1].lastRow);
            Assert::AreEqual (2 * PrinterGrid::kPageRows,     pages[2].firstRow);
            Assert::AreEqual (rows - 1,                       pages[2].lastRow);
        }


        TEST_METHOD (FormFeedForcesEarlyBreak)
        {
            // Form feed at row 800 splits a 2000-row strip into a short page
            // then its remainder (each under one physical page).
            auto   pages = PrintPagination::Paginate (Make (2000, { 800 }));

            Assert::AreEqual ((size_t) 2, pages.size());
            Assert::AreEqual (0,    pages[0].firstRow);
            Assert::AreEqual (799,  pages[0].lastRow);
            Assert::AreEqual (800,  pages[1].firstRow);
            Assert::AreEqual (1999, pages[1].lastRow);
        }


        TEST_METHOD (FormFeedAtPageHeightDoesNotDoubleCount)
        {
            // A form feed exactly at the page cap coincides with the implicit
            // break, so dedup keeps it two pages, not three.
            auto   pages = PrintPagination::Paginate (
                Make (2 * PrinterGrid::kPageRows, { PrinterGrid::kPageRows }));

            Assert::AreEqual ((size_t) 2, pages.size());
        }


        TEST_METHOD (BoundariesOutsideStripIgnored)
        {
            // 0, ==rowsUsed, and beyond are all no-ops.
            auto   pages = PrintPagination::Paginate (Make (500, { 0, 500, 900 }));

            Assert::AreEqual ((size_t) 1, pages.size());
            Assert::AreEqual (499, pages[0].lastRow);
        }
    };
}
