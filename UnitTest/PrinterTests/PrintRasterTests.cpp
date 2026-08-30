#include "Pch.h"

#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  PrintRasterTests
//
//  Cell striking and overprint accumulation, paper feed / blank-line
//  preservation, form-feed boundary recording, the 60-page cap, and clear.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrintRasterTests
{
    TEST_CLASS (PrintRasterTests)
    {
    public:

        TEST_METHOD (StrikeStoresInkAndExtendsUsed)
        {
            PrintRaster   raster;

            raster.Strike (10, 5, InkPrimary::Black);

            Assert::AreEqual ((Byte) InkPrimary::Black, raster.GetCell (10, 5));
            Assert::AreEqual (6, raster.GetRowsUsed());
        }


        TEST_METHOD (OverprintOrsPrimariesIntoComposite)
        {
            PrintRaster   raster;

            raster.Strike (3, 0, InkPrimary::Yellow);
            raster.Strike (3, 0, InkPrimary::Red);

            // Yellow (0x2) | Red (0x4) = 0x6 -- the renderer derives orange.
            Assert::AreEqual ((Byte) 0x6, raster.GetCell (3, 0));
        }


        TEST_METHOD (OutOfRangeColumnIgnored)
        {
            PrintRaster   raster;

            raster.Strike (-1, 0, InkPrimary::Black);
            raster.Strike (PrinterGrid::kDotsPerRow, 0, InkPrimary::Black);

            Assert::AreEqual (0, raster.GetRowsUsed());
        }


        TEST_METHOD (AdvanceRowsMovesPaperAndExtent)
        {
            PrintRaster   raster;

            raster.AdvanceRows (16);

            Assert::AreEqual (16, raster.GetPaperRow());
            Assert::AreEqual (16, raster.GetRowsUsed());
        }


        TEST_METHOD (BlankLineFeedsPreservedInExtent)
        {
            PrintRaster   raster;

            raster.Strike (0, 0, InkPrimary::Black);
            raster.AdvanceRows (12);
            raster.AdvanceRows (12);

            // Two blank feeds past the struck row must still count.
            Assert::AreEqual (24, raster.GetRowsUsed());
            Assert::AreEqual ((Byte) 0, raster.GetCell (0, 12));
        }


        TEST_METHOD (FormFeedRecordsBoundaryAndAdvancesToPageTop)
        {
            PrintRaster   raster;

            raster.AdvanceRows (200);
            raster.MarkFormFeed();

            Assert::AreEqual (PrinterGrid::kPageRows, raster.GetPaperRow());
            Assert::AreEqual ((size_t) 1, raster.GetPageBoundaryRows().size());
            Assert::AreEqual (PrinterGrid::kPageRows, raster.GetPageBoundaryRows()[0]);
        }


        TEST_METHOD (FormFeedFromPageBoundaryGoesToNextPage)
        {
            PrintRaster   raster;

            raster.AdvanceRows (PrinterGrid::kPageRows);   // exactly at a boundary
            raster.MarkFormFeed();

            Assert::AreEqual (PrinterGrid::kPageRows * 2, raster.GetPaperRow());
        }


        TEST_METHOD (CapReachedBeyondSixtyPages)
        {
            PrintRaster   raster;

            Assert::IsFalse (raster.HasReachedCap());

            raster.Strike (0, PrinterGrid::kMaxStripRows, InkPrimary::Black);

            Assert::IsTrue (raster.HasReachedCap());
            Assert::AreEqual (0, raster.GetRowsUsed());   // the capped strike was dropped
        }


        TEST_METHOD (ClearResetsEverything)
        {
            PrintRaster   raster;

            raster.Strike (5, 5, InkPrimary::Blue);
            raster.AdvanceRows (100);
            raster.MarkFormFeed();

            raster.Clear();

            Assert::AreEqual (0, raster.GetRowsUsed());
            Assert::AreEqual (0, raster.GetPaperRow());
            Assert::IsFalse (raster.HasReachedCap());
            Assert::AreEqual ((size_t) 0, raster.GetPageBoundaryRows().size());
            Assert::AreEqual ((Byte) 0, raster.GetCell (5, 5));
        }


        TEST_METHOD (CellAtUnallocatedIsPaperWhite)
        {
            PrintRaster   raster;

            Assert::AreEqual ((Byte) 0, raster.GetCell (100, 100));
        }


        TEST_METHOD (CopyRowSpanRebasesCells)
        {
            PrintRaster   raster;
            PrintRaster   span;

            raster.Strike (10, 100, InkPrimary::Black);
            raster.Strike (20, 150, InkPrimary::Red);
            raster.Strike (30, 200, InkPrimary::Blue);   // outside the span

            raster.CopyRowSpan (100, 199, span);

            Assert::AreEqual (100, span.GetRowsUsed());
            Assert::AreEqual ((Byte) InkPrimary::Black, span.GetCell (10, 0));    // 100 -> 0
            Assert::AreEqual ((Byte) InkPrimary::Red,   span.GetCell (20, 50));   // 150 -> 50
            Assert::AreEqual ((Byte) 0,                 span.GetCell (30, 100));  // 200 excluded
        }


        TEST_METHOD (CopyRowSpanClampsToUsedExtent)
        {
            PrintRaster   raster;
            PrintRaster   span;

            raster.Strike (0, 49, InkPrimary::Black);   // rowsUsed == 50

            raster.CopyRowSpan (-10, 500, span);

            Assert::AreEqual (50, span.GetRowsUsed());
            Assert::AreEqual ((Byte) InkPrimary::Black, span.GetCell (0, 49));
        }


        TEST_METHOD (CopyRowSpanEmptyWhenOutOfRange)
        {
            PrintRaster   raster;
            PrintRaster   span;

            raster.Strike (0, 9, InkPrimary::Black);

            raster.CopyRowSpan (100, 200, span);   // entirely past the extent

            Assert::AreEqual (0, span.GetRowsUsed());
        }


        TEST_METHOD (CopyRowSpanCarriesRebasedBoundaries)
        {
            PrintRaster   raster;
            PrintRaster   span;

            raster.Strike (0, 10, InkPrimary::Black);
            raster.MarkFormFeed();                       // boundary at kPageRows
            raster.Strike (0, PrinterGrid::kPageRows + 50, InkPrimary::Black);

            raster.CopyRowSpan (PrinterGrid::kPageRows - 100,
                                PrinterGrid::kPageRows + 50, span);

            Assert::AreEqual ((size_t) 1, span.GetPageBoundaryRows().size());
            Assert::AreEqual (100, span.GetPageBoundaryRows()[0]);   // rebased
        }


        TEST_METHOD (CopyRowSpanCopiesAdvancedBlankRowsAsWhite)
        {
            PrintRaster   raster;
            PrintRaster   span;

            raster.Strike      (0, 0, InkPrimary::Black);
            raster.AdvanceRows (5000);   // blank feed beyond the allocated store

            raster.CopyRowSpan (4000, 4999, span);

            Assert::AreEqual (1000, span.GetRowsUsed());
            Assert::AreEqual ((Byte) 0, span.GetCell (0, 500));
        }
    };
}
