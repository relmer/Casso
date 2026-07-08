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

            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (10, 5));
            Assert::AreEqual (6, raster.RowsUsed ());
        }


        TEST_METHOD (OverprintOrsPrimariesIntoComposite)
        {
            PrintRaster   raster;

            raster.Strike (3, 0, InkPrimary::Yellow);
            raster.Strike (3, 0, InkPrimary::Red);

            // Yellow (0x2) | Red (0x4) = 0x6 -- the renderer derives orange.
            Assert::AreEqual ((Byte) 0x6, raster.CellAt (3, 0));
        }


        TEST_METHOD (OutOfRangeColumnIgnored)
        {
            PrintRaster   raster;

            raster.Strike (-1, 0, InkPrimary::Black);
            raster.Strike (PrinterGrid::kDotsPerRow, 0, InkPrimary::Black);

            Assert::AreEqual (0, raster.RowsUsed ());
        }


        TEST_METHOD (AdvanceRowsMovesPaperAndExtent)
        {
            PrintRaster   raster;

            raster.AdvanceRows (16);

            Assert::AreEqual (16, raster.PaperRow ());
            Assert::AreEqual (16, raster.RowsUsed ());
        }


        TEST_METHOD (BlankLineFeedsPreservedInExtent)
        {
            PrintRaster   raster;

            raster.Strike (0, 0, InkPrimary::Black);
            raster.AdvanceRows (12);
            raster.AdvanceRows (12);

            // Two blank feeds past the struck row must still count.
            Assert::AreEqual (24, raster.RowsUsed ());
            Assert::AreEqual ((Byte) 0, raster.CellAt (0, 12));
        }


        TEST_METHOD (FormFeedRecordsBoundaryAndAdvancesToPageTop)
        {
            PrintRaster   raster;

            raster.AdvanceRows (200);
            raster.MarkFormFeed ();

            Assert::AreEqual (PrinterGrid::kPageRows, raster.PaperRow ());
            Assert::AreEqual ((size_t) 1, raster.PageBoundaryRows ().size ());
            Assert::AreEqual (PrinterGrid::kPageRows, raster.PageBoundaryRows ()[0]);
        }


        TEST_METHOD (FormFeedFromPageBoundaryGoesToNextPage)
        {
            PrintRaster   raster;

            raster.AdvanceRows (PrinterGrid::kPageRows);   // exactly at a boundary
            raster.MarkFormFeed ();

            Assert::AreEqual (PrinterGrid::kPageRows * 2, raster.PaperRow ());
        }


        TEST_METHOD (CapReachedBeyondSixtyPages)
        {
            PrintRaster   raster;

            Assert::IsFalse (raster.CapReached ());

            raster.Strike (0, PrinterGrid::kMaxStripRows, InkPrimary::Black);

            Assert::IsTrue (raster.CapReached ());
            Assert::AreEqual (0, raster.RowsUsed ());   // the capped strike was dropped
        }


        TEST_METHOD (ClearResetsEverything)
        {
            PrintRaster   raster;

            raster.Strike (5, 5, InkPrimary::Blue);
            raster.AdvanceRows (100);
            raster.MarkFormFeed ();

            raster.Clear ();

            Assert::AreEqual (0, raster.RowsUsed ());
            Assert::AreEqual (0, raster.PaperRow ());
            Assert::IsFalse (raster.CapReached ());
            Assert::AreEqual ((size_t) 0, raster.PageBoundaryRows ().size ());
            Assert::AreEqual ((Byte) 0, raster.CellAt (5, 5));
        }


        TEST_METHOD (CellAtUnallocatedIsPaperWhite)
        {
            PrintRaster   raster;

            Assert::AreEqual ((Byte) 0, raster.CellAt (100, 100));
        }
    };
}
