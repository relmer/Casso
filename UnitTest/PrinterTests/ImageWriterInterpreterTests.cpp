#include "Pch.h"

#include "Devices/Printer/ImageWriterInterpreter.h"
#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  ImageWriterInterpreterTests
//
//  Parser mechanics and the confident command subset: CR/LF/FF, line spacing,
//  bit-image striking geometry (per the interpreter's provisional convention),
//  unknown-command reporting, reset, and determinism. Real captured Print Shop
//  fixture replay is added at the T011 checkpoint.
//
////////////////////////////////////////////////////////////////////////////////

namespace ImageWriterInterpreterTests
{
    static void Feed (ImageWriterInterpreter & interp, PrintRaster & raster,
                      vector<PrinterEvent> & events, const vector<Byte> & bytes)
    {
        interp.Consume (bytes.data (), bytes.size (), raster, events);
    }


    static int CountEvents (const vector<PrinterEvent> & events, PrinterEventType type)
    {
        int   n = 0;
        size_t i = 0;
        for (i = 0; i < events.size (); i++)
        {
            if (events[i].type == type) n++;
        }
        return n;
    }




    TEST_CLASS (ImageWriterInterpreterTests)
    {
    public:

        TEST_METHOD (LineFeedAdvancesPaperAtDefaultSpacing)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0x0A });     // LF

            Assert::AreEqual (PrinterGrid::kRowsPerInch / 6, raster.PaperRow ());   // 24
            Assert::AreEqual (1, CountEvents (events, PrinterEventType::LineFeed));
            Assert::AreEqual (PrinterGrid::kRowsPerInch / 6, events[0].rows);
        }


        TEST_METHOD (FormFeedMarksBoundary)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0x0C });     // FF

            Assert::AreEqual (PrinterGrid::kPageRows, raster.PaperRow ());
            Assert::AreEqual (1, CountEvents (events, PrinterEventType::FormFeed));
        }


        TEST_METHOD (EscALineSpacingIsSixLpi)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0x1B, 'B', 0x0A });   // 8 lpi then LF
            Assert::AreEqual (PrinterGrid::kRowsPerInch / 8, raster.PaperRow ());   // 18

            Feed (interp, raster, events, { 0x1B, 'A', 0x0A });   // back to 6 lpi then LF
            Assert::AreEqual (PrinterGrid::kRowsPerInch / 8 + PrinterGrid::kRowsPerInch / 6, raster.PaperRow ());
        }


        TEST_METHOD (EscTSetsCustomLineSpacing)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0x1B, 'T', '1', '8', 0x0A });   // 18/144" then LF

            Assert::AreEqual (18, raster.PaperRow ());
        }


        TEST_METHOD (BitImageStrikesEightVerticalDots)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            // ESC G 0002, then bottom-pin column (0x80) and top-pin column (0x01).
            // Pins are 1/72" apart -> 2 rows each: bottom pin -> rows 14..15,
            // top pin -> rows 0..1.
            Feed (interp, raster, events, { 0x1B, 'G', '0', '0', '0', '2', 0x80, 0x01 });

            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (0, 14));  // MSB -> bottom pin
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (0, 15));  // ...fills 2 rows
            Assert::AreEqual ((Byte) 0,                 raster.CellAt (0, 0));
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (1, 0));   // LSB -> top pin
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (1, 1));   // ...fills 2 rows
            Assert::AreEqual ((Byte) 0,                 raster.CellAt (1, 14));

            Assert::AreEqual (1, CountEvents (events, PrinterEventType::HeadBurst));
            Assert::AreEqual (0, events.back ().fromDot);
            Assert::AreEqual (1, events.back ().toDot);
        }


        TEST_METHOD (CarriageReturnResetsHeadColumn)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            // One graphics column (head -> 1), CR (head -> 0), another column.
            Feed (interp, raster, events, { 0x1B, 'G', '0', '0', '0', '1', 0x80 });
            Feed (interp, raster, events, { 0x0D });
            Feed (interp, raster, events, { 0x1B, 'G', '0', '0', '0', '1', 0x80 });

            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (0, 14));  // 0x80 -> bottom pin (rows 14..15)
            Assert::AreEqual ((Byte) 0,                 raster.CellAt (1, 14));  // CR sent it back to col 0
        }


        TEST_METHOD (PrintableAsciiIsConsumedNotRendered)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 'H', 'e', 'l', 'l', 'o' });

            Assert::AreEqual (0, raster.RowsUsed ());        // no strikes, no advance
            Assert::AreEqual ((size_t) 0, events.size ());
        }


        TEST_METHOD (UnknownEscCommandIsReported)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0x1B, 0x7A });   // ESC 'z' -- not in the subset

            Assert::AreEqual (1, CountEvents (events, PrinterEventType::UnknownCommand));
            Assert::AreEqual ((Byte) 0x7A, events.back ().leadByte);
        }


        TEST_METHOD (ResetRestoresDefaultsAndReports)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            // Change line spacing, then reset, then LF -- spacing is back to default.
            Feed (interp, raster, events, { 0x1B, 'T', '9', '9' });   // 99/144"
            Feed (interp, raster, events, { 0x1B, 'c' });             // reset
            Feed (interp, raster, events, { 0x0A });                  // LF at default spacing

            Assert::AreEqual (1, CountEvents (events, PrinterEventType::ResetSeen));
            Assert::AreEqual (PrinterGrid::kRowsPerInch / 6, raster.PaperRow ());   // default 24
        }


        TEST_METHOD (DeterministicForIdenticalStream)
        {
            vector<Byte>   stream = { 0x1B, 'G', '0', '0', '0', '3', 0x80, 0xFF, 0x01,
                                      0x0D, 0x0A, 0x1B, 'T', '1', '2', 0x0A, 0x0C };

            ImageWriterInterpreter   a;
            ImageWriterInterpreter   b;
            PrintRaster              ra;
            PrintRaster              rb;
            vector<PrinterEvent>     ea;
            vector<PrinterEvent>     eb;
            size_t                   i = 0;

            Feed (a, ra, ea, stream);
            Feed (b, rb, eb, stream);

            Assert::AreEqual (ra.RowsUsed (), rb.RowsUsed ());
            Assert::AreEqual (ra.PaperRow (), rb.PaperRow ());
            Assert::AreEqual (ea.size (),     eb.size ());

            for (i = 0; i < ea.size (); i++)
            {
                Assert::IsTrue (ea[i].type == eb[i].type);
                Assert::AreEqual (ea[i].fromDot, eb[i].fromDot);
                Assert::AreEqual (ea[i].toDot,   eb[i].toDot);
                Assert::AreEqual (ea[i].rows,    eb[i].rows);
            }
        }
    };
}
