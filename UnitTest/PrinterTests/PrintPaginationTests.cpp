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


        //  ---- FitFullPageToBox -----------------------------------------------

        TEST_METHOD (FullPageFitsToHeightNotWidth)
        {
            // A full 8"x11" page (800x1100 px at 100 dpi) into a Letter box
            // (850x1100). Width alone would over-scale to 1.0625 and spill the
            // bottom; capping by height prints it 1:1, centered.
            PrintPagination::PageFit   fit = PrintPagination::FitFullPageToBox (800, 1100, 850, 1100, 100.0);

            Assert::AreEqual (1.0,    fit.scale, 0.001, L"full page prints at true size, height-capped");
            Assert::AreEqual (1100.0, fit.destH, 0.5,   L"the whole page height fits the sheet");
            Assert::AreEqual (800.0,  fit.destW, 0.5,   L"width scaled by the same factor");
        }


        TEST_METHOD (NarrowBoxCapsByWidth)
        {
            // When the box is narrower than the page's width scale allows, width
            // wins so nothing spills off the right edge.
            PrintPagination::PageFit   fit = PrintPagination::FitFullPageToBox (800, 1100, 400, 1100, 100.0);

            Assert::AreEqual (0.5,   fit.scale, 0.001, L"width caps the scale");
            Assert::AreEqual (400.0, fit.destW, 0.5,   L"content fits the box width");
        }


        TEST_METHOD (ShortLastPageSharesFullPageScale)
        {
            // The fanfold guarantee: a short last page (200 px tall) is NOT blown
            // up to fill the sheet -- it uses the SAME scale as a full page, so its
            // columns line up with the pages above it.
            PrintPagination::PageFit   full  = PrintPagination::FitFullPageToBox (800, 1100, 850, 1100, 100.0);
            PrintPagination::PageFit   shortP = PrintPagination::FitFullPageToBox (800, 200, 850, 1100, 100.0);

            Assert::AreEqual (full.scale, shortP.scale, 0.001, L"every page shares one scale");
            Assert::AreEqual (200.0, shortP.destH, 0.5, L"the short page stays short, top-aligned");
        }


        TEST_METHOD (DipUnitsMatchPixelUnits)
        {
            // The DIP path (96 units/inch) and the device-pixel path must produce
            // the same scale for the same physical page, so preview == print.
            PrintPagination::PageFit   dip = PrintPagination::FitFullPageToBox (768, 1056, 816, 1056, 96.0);

            Assert::AreEqual (1.0, dip.scale, 0.001, L"DIP full page also prints at true size");
        }


        TEST_METHOD (ZeroContentDoesNotDivideByZero)
        {
            PrintPagination::PageFit   fit = PrintPagination::FitFullPageToBox (0, 0, 850, 1100, 100.0);

            Assert::IsTrue (fit.scale > 0.0, L"a degenerate page yields a finite, positive scale");
        }
    };
}
