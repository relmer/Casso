#include "Pch.h"

#include "Devices/Printer/PrintJobSerializer.h"
#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintJobSerializerTests
//
//  Round-trip of a strip through the index plane + JSON metadata, and rejection
//  of corrupt / unknown-version sidecars.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrintJobSerializerTests
{
    static void BuildSampleStrip (PrintRaster & raster)
    {
        raster.Strike (0,   0, InkPrimary::Black);
        raster.Strike (100, 3, InkPrimary::Yellow);
        raster.Strike (100, 3, InkPrimary::Red);       // orange composite (0x6)
        raster.Strike (1279, 5, InkPrimary::Blue);
        raster.AdvanceRows (200);
        raster.MarkFormFeed();                        // boundary at 1584, paperRow=1584
    }




    TEST_CLASS (PrintJobSerializerTests)
    {
    public:

        TEST_METHOD (RoundTripPreservesCellsAndState)
        {
            PrintRaster    original;
            PrintRaster    rebuilt;
            StripMeta      meta;
            vector<Byte>   pixels;
            int            w = 0;
            int            h = 0;

            BuildSampleStrip (original);

            PrintJobSerializer::ExtractIndexPlane (original, w, h, pixels);
            string   json = PrintJobSerializer::WriteMetaJson (original);

            Assert::IsTrue (SUCCEEDED (PrintJobSerializer::ReadMetaJson (json, meta)));
            Assert::IsTrue (SUCCEEDED (PrintJobSerializer::RebuildRaster (w, h, pixels, meta, rebuilt)));

            Assert::AreEqual (original.RowsUsed(),   rebuilt.RowsUsed());
            Assert::AreEqual (original.PaperRow(),   rebuilt.PaperRow());
            Assert::AreEqual (original.CapReached(), rebuilt.CapReached());

            Assert::AreEqual (original.PageBoundaryRows().size(), rebuilt.PageBoundaryRows().size());
            Assert::AreEqual (PrinterGrid::kPageRows, rebuilt.PageBoundaryRows()[0]);

            // Struck cells (including the orange composite) survive intact.
            Assert::AreEqual ((Byte) InkPrimary::Black, rebuilt.CellAt (0, 0));
            Assert::AreEqual ((Byte) 0x6,               rebuilt.CellAt (100, 3));
            Assert::AreEqual ((Byte) InkPrimary::Blue,  rebuilt.CellAt (1279, 5));
            Assert::AreEqual ((Byte) 0,                 rebuilt.CellAt (50, 50));
        }


        TEST_METHOD (PlaneDimensionsAreNativeGrid)
        {
            PrintRaster    raster;
            vector<Byte>   pixels;
            int            w = 0;
            int            h = 0;

            raster.Strike (0, 9, InkPrimary::Black);
            PrintJobSerializer::ExtractIndexPlane (raster, w, h, pixels);

            Assert::AreEqual (PrinterGrid::kDotsPerRow, w);
            Assert::AreEqual (10, h);                              // rowsUsed
            Assert::AreEqual ((size_t) w * h, pixels.size());
        }


        TEST_METHOD (CorruptJsonIsRejected)
        {
            StripMeta   meta;

            Assert::IsFalse (SUCCEEDED (PrintJobSerializer::ReadMetaJson ("{ not valid", meta)));
        }


        TEST_METHOD (UnknownFormatVersionIsRejected)
        {
            StripMeta   meta;
            string      json = "{ \"formatVersion\": 999, \"rowsUsed\": 0, "
                               "\"paperRow\": 0, \"capReached\": false }";

            Assert::IsFalse (SUCCEEDED (PrintJobSerializer::ReadMetaJson (json, meta)));
        }


        TEST_METHOD (RebuildRejectsWrongWidth)
        {
            PrintRaster    raster;
            StripMeta      meta;
            vector<Byte>   pixels (64, 0);

            Assert::IsFalse (SUCCEEDED (
                PrintJobSerializer::RebuildRaster (8, 8, pixels, meta, raster)));
        }
    };
}
