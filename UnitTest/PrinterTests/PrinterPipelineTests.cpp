#include "Pch.h"

#include "Devices/Printer/ImageWriterInterpreter.h"
#include "Devices/Printer/PrintRaster.h"
#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PrintJobSerializer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterPipelineTests
//
//  Integration proof that the pure CassoEmuCore printer components compose:
//  a synthetic ImageWriter stream drives the interpreter, strikes a raster,
//  renders to ink pixels, and survives a persistence round-trip -- the whole
//  bytes -> image path exercised without any shell or system dependency.
//
////////////////////////////////////////////////////////////////////////////////

namespace PrinterPipelineTests
{
    // Three stacked bands of solid bit-image graphics, each a full 8-pin
    // column run followed by CR/LF -- a stand-in for a small printed sign.
    static void BuildSignStream (vector<Byte> & out)
    {
        const int   kCols  = 20;
        const int   kLines = 3;
        int         line   = 0;
        int         i      = 0;

        for (line = 0; line < kLines; line++)
        {
            out.push_back (0x1B);            // ESC
            out.push_back ('G');
            out.push_back ('0');             // count = 0020
            out.push_back ('0');
            out.push_back ('2');
            out.push_back ('0');
            for (i = 0; i < kCols; i++)
            {
                out.push_back (0xFF);        // all eight pins
            }
            out.push_back (0x0D);            // CR
            out.push_back (0x0A);            // LF
        }
    }




    TEST_CLASS (PrinterPipelineTests)
    {
    public:

        TEST_METHOD (StreamToRasterToImage)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              raster;
            vector<PrinterEvent>     events;
            vector<Byte>             stream;

            BuildSignStream (stream);
            interp.Consume (stream.data (), stream.size (), raster, events);

            // First band occupies rows 0..7, columns 0..19.
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (0, 0));
            Assert::AreEqual ((Byte) InkPrimary::Black, raster.CellAt (19, 7));
            Assert::AreEqual ((Byte) 0,                 raster.CellAt (20, 0));
            Assert::IsTrue (raster.RowsUsed () > 0);

            // Render the whole strip and confirm ink actually landed.
            {
                PaperRenderer            renderer;
                PaperRenderer::Options   opt;
                RgbaImage                img;
                int                      inked = 0;
                int                      x = 0, y = 0;

                opt.outputDpi = 288;
                Assert::IsTrue (SUCCEEDED (
                    renderer.Render (raster, 0, raster.RowsUsed () - 1, opt, img)));

                for (y = 0; y < img.height; y++)
                {
                    for (x = 0; x < img.width; x++)
                    {
                        const Byte *   p = img.PixelAt (x, y);
                        if (p[0] < 250 || p[1] < 250 || p[2] < 250) inked++;
                    }
                }

                Assert::IsTrue (inked > 0, L"rendered strip has no ink");
            }
        }


        TEST_METHOD (StreamSurvivesPersistenceRoundTrip)
        {
            ImageWriterInterpreter   interp;
            PrintRaster              original;
            PrintRaster              reloaded;
            vector<PrinterEvent>     events;
            vector<Byte>             stream;
            vector<Byte>             plane;
            StripMeta                meta;
            int                      w = 0, h = 0;

            BuildSignStream (stream);
            interp.Consume (stream.data (), stream.size (), original, events);

            PrintJobSerializer::ExtractIndexPlane (original, w, h, plane);
            string   json = PrintJobSerializer::WriteMetaJson (original);

            Assert::IsTrue (SUCCEEDED (PrintJobSerializer::ReadMetaJson (json, meta)));
            Assert::IsTrue (SUCCEEDED (PrintJobSerializer::RebuildRaster (w, h, plane, meta, reloaded)));

            Assert::AreEqual (original.RowsUsed (),        reloaded.RowsUsed ());
            Assert::AreEqual (original.CellAt (0, 0),      reloaded.CellAt (0, 0));
            Assert::AreEqual (original.CellAt (19, 7),     reloaded.CellAt (19, 7));
        }
    };
}
