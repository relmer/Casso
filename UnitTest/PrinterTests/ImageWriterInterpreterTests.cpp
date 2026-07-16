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
        interp.Consume (bytes.data(), bytes.size(), raster, events);
    }


    static int CountEvents (const vector<PrinterEvent> & events, PrinterEventType type)
    {
        int   n = 0;
        size_t i = 0;
        for (i = 0; i < events.size(); i++)
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

            Assert::AreEqual (PrinterGrid::kRowsPerInch / 6, raster.PaperRow());   // 24
            Assert::AreEqual (1, CountEvents (events, PrinterEventType::LineFeed));
            Assert::AreEqual (PrinterGrid::kRowsPerInch / 6, events[0].rows);
        }


        TEST_METHOD (FormFeedMarksBoundary)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0x0C });     // FF

            Assert::AreEqual (PrinterGrid::kPageRows, raster.PaperRow());
            Assert::AreEqual (1, CountEvents (events, PrinterEventType::FormFeed));
        }


        TEST_METHOD (EscASetsLineSpacingIn72nds)
        {
            // ESC A n: ONE binary parameter, line feed = n/72" (T011 capture:
            // Print Shop's sign passes feed at ESC A $07 = 14 native rows;
            // its text uses ESC A $0C = 1/6").
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0x1B, 'B', 0x0A });   // 8 lpi then LF
            Assert::AreEqual (PrinterGrid::kRowsPerInch / 8, raster.PaperRow());   // 18

            Feed (interp, raster, events, { 0x1B, 'A', 0x0C, 0x0A });   // 12/72" = 1/6" then LF
            Assert::AreEqual (18 + PrinterGrid::kRowsPerInch / 6, raster.PaperRow());

            Feed (interp, raster, events, { 0x1B, 'A', 0x07, 0x0A });   // 7/72" = 14 rows then LF
            Assert::AreEqual (18 + PrinterGrid::kRowsPerInch / 6 + 14, raster.PaperRow());
        }


        TEST_METHOD (EscTSetsCustomLineSpacing)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0x1B, 'T', '1', '8', 0x0A });   // 18/144" then LF

            Assert::AreEqual (18, raster.PaperRow());
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
            Assert::AreEqual (0, events.back().fromDot);
            Assert::AreEqual (1, events.back().toDot);
        }


        TEST_METHOD (BinaryCountBitImageStrikesLikeEscG)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            // ESC L with a BINARY little-endian count -- the form Print
            // Shop's ImageWriter driver sends for its setup welcome message
            // (T011 capture: ESC L $00 $02 + 512 columns). Pin order is the
            // REVERSE of ESC G: the MSB is the top pin (the welcome message
            // prints upside down otherwise). Columns are 120 dpi, so the
            // first two land on native dots 0 and 1.
            Feed (interp, raster, events, { 0x1B, 'L', 0x02, 0x00, 0x80, 0x01 });

            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (0, 0));   // MSB -> TOP pin
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (1, 14));  // LSB -> bottom pin
            Assert::AreEqual ((Byte) 0,                 raster.CellAt (0, 14));

            Assert::AreEqual (1, CountEvents (events, PrinterEventType::HeadBurst));
            Assert::AreEqual (0, CountEvents (events, PrinterEventType::UnknownCommand));
            Assert::AreEqual (0, events.back().fromDot);
            Assert::AreEqual (1, events.back().toDot);
        }


        TEST_METHOD (BinaryCountBitImageHighByteScalesCount)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;
            vector<Byte>             stream = { 0x1B, 'L', 0x00, 0x01 };   // count = 256, LSB first

            for (int i = 0; i < 256; i++)
            {
                stream.push_back (0x80);   // top pin (MSB) all the way across
            }
            Feed (interp, raster, events, stream);

            // 120-dpi columns on the 160-dpi grid: 256 columns span
            // 256*4/3 = 341 native dots, gap-free.
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (0,   0));
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (170, 0));
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (340, 0));
            Assert::AreEqual ((Byte) 0,                 raster.CellAt (341, 0));
            Assert::AreEqual (1, CountEvents (events, PrinterEventType::HeadBurst));
            Assert::AreEqual (340, events.back().toDot);   // all 256 columns were data, not text
        }


        TEST_METHOD (PrintShopWelcomeMessagePrefixRendersInk)
        {
            // The exact prefix Print Shop's setup test sends (T011 capture):
            // CR CR, ESC A $0C (1/6" spacing), LF, then ESC L $00 $02 + 512
            // graphics columns, CR LF. The band lands one line feed down and
            // spans 512*4/3 = 682 native dots (120-dpi columns).
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;
            vector<Byte>             stream = { 0x0D, 0x0D, 0x1B, 'A', 0x0C, 0x0A, 0x1B, 'L', 0x00, 0x02 };

            for (int i = 0; i < 512; i++)
            {
                stream.push_back (0x7F);   // all pins but the top (MSB clear)
            }
            stream.push_back (0x0D);
            stream.push_back (0x0A);
            Feed (interp, raster, events, stream);

            int   bandTop = PrinterGrid::kRowsPerInch / 6;   // one 1/6" line feed

            Assert::AreEqual (1, CountEvents (events, PrinterEventType::HeadBurst));
            Assert::AreEqual (0, CountEvents (events, PrinterEventType::UnknownCommand));
            Assert::AreEqual ((Byte) 0,                 raster.CellAt (0,   bandTop));      // MSB clear -> top pin empty
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (0,   bandTop + 2));  // bit 6 -> second pin
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (681, bandTop + 2));  // last column's dot
            Assert::IsTrue (raster.RowsUsed() > bandTop,
                L"the graphics band must extend the used-rows extent");
        }


        TEST_METHOD (PrintShopSignPassGeometry)
        {
            // One sign pass from the T011 sign capture: ESC A $07 spacing,
            // then ESC L 96 + ESC L 864 back to back (one 960-column line =
            // the full 8" printable width at 120 dpi), CR LF. The two bursts
            // must butt seamlessly and the feed must be 14 rows.
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;
            vector<Byte>             stream = { 0x1B, 'A', 0x07, 0x1B, 'L', 0x60, 0x00 };

            for (int i = 0; i < 96; i++)
            {
                stream.push_back (0xFF);
            }
            stream.push_back (0x1B);
            stream.push_back ('L');
            stream.push_back (0x60);   // 864 = 0x0360
            stream.push_back (0x03);
            for (int i = 0; i < 864; i++)
            {
                stream.push_back (0xFF);
            }
            stream.push_back (0x0D);
            stream.push_back (0x0A);
            Feed (interp, raster, events, stream);

            // 960 columns * 4/3 = 1280 dots = the full printable width; the
            // seam at column 96 (dot 128) must carry ink from both bursts.
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (0,    0));
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (127,  0));
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (128,  0));
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (1279, 0));
            Assert::AreEqual (14, raster.PaperRow());   // ESC A $07 = 14 native rows
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

            Assert::AreEqual (0, raster.RowsUsed());        // no strikes, no advance
            Assert::AreEqual ((size_t) 0, events.size());
        }


        TEST_METHOD (UnknownEscCommandIsReported)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0x1B, 0x7A });   // ESC 'z' -- not in the subset

            Assert::AreEqual (1, CountEvents (events, PrinterEventType::UnknownCommand));
            Assert::AreEqual ((Byte) 0x7A, events.back().leadByte);
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
            Assert::AreEqual (PrinterGrid::kRowsPerInch / 6, raster.PaperRow());   // default 24
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

            Assert::AreEqual (ra.RowsUsed(), rb.RowsUsed());
            Assert::AreEqual (ra.PaperRow(), rb.PaperRow());
            Assert::AreEqual (ea.size(),     eb.size());

            for (i = 0; i < ea.size(); i++)
            {
                Assert::IsTrue (ea[i].type == eb[i].type);
                Assert::AreEqual (ea[i].fromDot, eb[i].fromDot);
                Assert::AreEqual (ea[i].toDot,   eb[i].toDot);
                Assert::AreEqual (ea[i].rows,    eb[i].rows);
            }
        }
    };
}
