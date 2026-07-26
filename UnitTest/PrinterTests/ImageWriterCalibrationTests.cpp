#include "Pch.h"

#include "Devices/Printer/ImageWriterInterpreter.h"
#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

// Geometry/colour pinned against a real The Print Shop Color capture
// (specs/015-printer-support/captures/printshop-color-testpage.bin, which
// renders as "WELCOME TO \"THE PRINT SHOP\""). These lock the calibrated
// behaviours in memory so the fixture itself is not a test dependency:
//   - ESC K n colour select maps 0..6 to the ribbon primaries/composites
//   - ESC G bit-image packs the LSB as the TOP pin
//   - overprinting ORs primaries into a cell (composite colours)
//   - ESC '>' / ESC '<' / ESC 'P' are recognised no-op preamble

namespace ImageWriterCalibrationTests
{
    static const Byte   s_kEsc = 0x1B;

    static Byte RunSingleColumn (const vector<Byte> & stream, int row)
    {
        PrintRaster              raster;
        ImageWriterInterpreter   interp;
        vector<PrinterEvent>     events;

        interp.Reset();
        interp.Consume (stream.data(), stream.size(), raster, events);
        return raster.CellAt (0, row);
    }


    TEST_CLASS (ImageWriterCalibrationTests)
    {
    public:

        //  ESC G byte 0x01 (LSB) lands on the TOP pin, 0x80 (MSB) on the bottom
        //  pin. Pins are 1/72" apart -- two rows on the 144-row grid -- so each
        //  pin fills a 2-row dot: top pin -> rows 0..1, bottom pin -> rows 14..15.
        TEST_METHOD (GraphicsBitOrderLsbIsTopPin)
        {
            vector<Byte>   lsb = { s_kEsc, 'K', '1', s_kEsc, 'G', '0', '0', '0', '1', 0x01 };
            vector<Byte>   msb = { s_kEsc, 'K', '1', s_kEsc, 'G', '0', '0', '0', '1', 0x80 };

            Assert::AreEqual ((int) InkPrimary::Yellow, (int) RunSingleColumn (lsb, 0),  L"LSB should strike top pin (row 0)");
            Assert::AreEqual ((int) InkPrimary::Yellow, (int) RunSingleColumn (lsb, 1),  L"top pin fills a 2-row dot (row 1)");
            Assert::AreEqual (0,                        (int) RunSingleColumn (lsb, 14), L"LSB must not strike bottom");

            Assert::AreEqual ((int) InkPrimary::Yellow, (int) RunSingleColumn (msb, 14), L"MSB should strike bottom pin (row 14)");
            Assert::AreEqual ((int) InkPrimary::Yellow, (int) RunSingleColumn (msb, 15), L"bottom pin fills a 2-row dot (row 15)");
            Assert::AreEqual (0,                        (int) RunSingleColumn (msb, 0),  L"MSB must not strike top");
        }


        //  ESC K 0..3 select the four ribbon primaries Print Shop drives.
        TEST_METHOD (ColorSelectMapsRibbonPrimaries)
        {
            auto strike = [] (char digit) -> int
            {
                vector<Byte>   s = { s_kEsc, 'K', (Byte) digit, s_kEsc, 'G', '0', '0', '0', '1', 0x01 };
                return (int) RunSingleColumn (s, 0);
            };

            Assert::AreEqual ((int) InkPrimary::Black,  strike ('0'));
            Assert::AreEqual ((int) InkPrimary::Yellow, strike ('1'));
            Assert::AreEqual ((int) InkPrimary::Red,    strike ('2'));
            Assert::AreEqual ((int) InkPrimary::Blue,   strike ('3'));
        }


        //  Two passes over the same cell (Print Shop's yellow then red) OR into
        //  the composite -- orange = Yellow|Red -- exactly as overprinting does.
        TEST_METHOD (OverprintingOrsIntoComposite)
        {
            vector<Byte>   twoPass =
            {
                s_kEsc, 'K', '1', s_kEsc, 'G', '0', '0', '0', '1', 0x01,   // yellow at col 0
                0x0D,                                                       // CR -> back to col 0
                s_kEsc, 'K', '2', s_kEsc, 'G', '0', '0', '0', '1', 0x01,   // red over the same cell
            };
            int   expected = (int) InkPrimary::Yellow | (int) InkPrimary::Red;

            Assert::AreEqual (expected, (int) RunSingleColumn (twoPass, 0));
        }


        //  The ESC '>' / ESC '<' / ESC 'P' preamble is consumed silently -- no
        //  UnknownCommand events -- and does not disturb the following pass.
        TEST_METHOD (DirectionAndPitchPreambleAreNoOps)
        {
            PrintRaster              raster;
            ImageWriterInterpreter   interp;
            vector<PrinterEvent>     events;
            vector<Byte>             stream =
            {
                s_kEsc, '>', s_kEsc, '<', s_kEsc, 'P',
                s_kEsc, 'K', '1', s_kEsc, 'G', '0', '0', '0', '1', 0x01,
            };

            interp.Reset();
            interp.Consume (stream.data(), stream.size(), raster, events);

            for (const PrinterEvent & e : events)
            {
                Assert::IsTrue (e.type != PrinterEventType::UnknownCommand, L"preamble must not be unknown");
            }
            Assert::AreEqual ((int) InkPrimary::Yellow, (int) raster.CellAt (0, 0));
        }
    };
}
