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


        TEST_METHOD (TextRendersDraftGlyphAtPica)
        {
            // US6: 'H' fed as high-bit ASCII ($C8 -- what PR#1 LIST actually
            // sends; the 8th data bit is ignored for text). Pica cell = 16
            // native dots, so glyph sub-column i spans dots [i*2, (i+1)*2);
            // each pin fills 2 rows (bit 0 = top, like ESC G).
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0xC8 });   // 'H' | 0x80

            Assert::IsTrue (raster.CellAt (2, 0)   != 0);   // left stem  (sub-col 1, top pin)
            Assert::IsTrue (raster.CellAt (3, 13)  != 0);   // left stem  (baseline pin, rows 12..13)
            Assert::IsTrue (raster.CellAt (6, 6)   != 0);   // crossbar   (sub-col 3, pin 3 -> rows 6..7)
            Assert::IsTrue (raster.CellAt (10, 0)  != 0);   // right stem (sub-col 5)
            Assert::IsTrue (raster.CellAt (0, 0)   == 0);   // pre-centered: sub-col 0 blank
            Assert::IsTrue (raster.CellAt (14, 0)  == 0);   // trailing inter-character gap blank
            Assert::AreEqual (16, interp.HeadColumnDots());  // advanced one pica cell
        }


        TEST_METHOD (TextSpaceAdvancesWithoutInk)
        {
            // DOS catalogs pad with $A0 (space | high bit): the head advances a
            // cell but nothing strikes.
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0xA0 });

            Assert::AreEqual (0,  raster.RowsUsed());
            Assert::AreEqual (16, interp.HeadColumnDots());
        }


        TEST_METHOD (TextBareCarriageReturnOverprintsWithoutFeed)
        {
            // BASIC sends bare CRs ($8D); the slot FIRMWARE injects the LF (like
            // Apple's parallel card), so at the interpreter a lone CR returns
            // the head without feeding -- overprint, exactly like the real
            // mechanism.
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;

            Feed (interp, raster, events, { 0xC1, 0x8D, 0xC2 });   // 'A' CR 'B'

            Assert::AreEqual (0,  raster.PaperRow());        // no feed
            Assert::AreEqual (16, interp.HeadColumnDots());  // 'B' printed from the left margin
        }


        TEST_METHOD (TextWrapsAtTheRightMargin)
        {
            // 80 pica characters exactly fill the 1280-dot line; the 81st wraps
            // with an implicit new line at the current spacing (default 1/6").
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;
            vector<Byte>             line (81, 0xD7);   // 'W' | 0x80

            Feed (interp, raster, events, line);

            Assert::AreEqual (PrinterGrid::kRowsPerInch / 6, raster.PaperRow());   // wrapped once
            Assert::AreEqual (16, interp.HeadColumnDots());                        // 81st char on the new line
            Assert::AreEqual (1,  CountEvents (events, PrinterEventType::LineFeed));
        }


        TEST_METHOD (PitchMatrixEveryDocumentedDensity)
        {
            // T057 / SC-005: every documented pitch command sets the expected
            // character cell width (native 160 dpi / cpi). One character is
            // rendered after each selection and the head advance asserted.
            static const struct { Byte cmd; int cpi; } s_kPitches[] =
            {
                { 'n',  9 },   // extended
                { 'N', 10 },   // pica
                { 'E', 12 },   // elite
                { 'e', 13 },   // semicondensed
                { 'q', 15 },   // condensed
                { 'Q', 17 },   // ultracondensed
            };

            for (const auto & p : s_kPitches)
            {
                ImageWriterInterpreter   interp;
                PrintRaster              raster;
                vector<PrinterEvent>     events;

                Feed (interp, raster, events, { 0x1B, p.cmd, 'M' });
                Assert::AreEqual (PrinterGrid::kDotsPerInchH / p.cpi, interp.HeadColumnDots());
            }
        }


        TEST_METHOD (TextPitchSelectionChangesCellWidth)
        {
            // ESC Q (ultracondensed, 160/17 = 9 dots/char) then ESC N (pica, 16)
            // -- the second sequence sent entirely high-bit set, as BASIC would.
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;
            int                      condensed = PrinterGrid::kDotsPerInchH / 17;
            int                      pica      = PrinterGrid::kDotsPerInchH / 10;

            Feed (interp, raster, events, { 0x1B, 'Q', 'M' });
            Assert::AreEqual (condensed, interp.HeadColumnDots());

            Feed (interp, raster, events, { 0x9B, 0xCE, 0xCD });   // ESC N 'M', high-bit
            Assert::AreEqual (condensed + pica, interp.HeadColumnDots());
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
