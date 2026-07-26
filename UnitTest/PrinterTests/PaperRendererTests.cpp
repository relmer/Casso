#include "Pch.h"

#include "Devices/Printer/PaperRenderer.h"
#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  PaperRendererTests
//
//  Output geometry, square-pixel fidelity (SC-009), deterministic bytes,
//  overprint composite colours, dot roundness, and the Plain style.
//
////////////////////////////////////////////////////////////////////////////////

namespace PaperRendererTests
{
    static bool IsInked (const RgbaImage & img, int x, int y)
    {
        const Byte *   p = img.PixelAt (x, y);
        return p[0] < 250 || p[1] < 250 || p[2] < 250;
    }


    static void InkBounds (const RgbaImage & img, int & minX, int & minY, int & maxX, int & maxY)
    {
        int   x = 0;
        int   y = 0;

        minX = img.width;
        minY = img.height;
        maxX = -1;
        maxY = -1;

        for (y = 0; y < img.height; y++)
        {
            for (x = 0; x < img.width; x++)
            {
                if (IsInked (img, x, y))
                {
                    if (x < minX) minX = x;
                    if (y < minY) minY = y;
                    if (x > maxX) maxX = x;
                    if (y > maxY) maxY = y;
                }
            }
        }
    }


    // The output pixel a cell's dot is centred on, mirroring the renderer's maths.
    static void CellCenter (int col, int rowLocal, int dpi, int & cx, int & cy)
    {
        cx = (2 * col + 1) * dpi / 320;
        cy = (2 * rowLocal + 1) * dpi / 288;
    }




    TEST_CLASS (PaperRendererTests)
    {
    public:

        TEST_METHOD (OutputDimensionsFollowDpiAndRows)
        {
            PrintRaster              raster;
            PaperRenderer            renderer;
            PaperRenderer::Options   opt;
            RgbaImage                img;

            opt.outputDpi = 576;
            Assert::IsTrue (SUCCEEDED (renderer.Render (raster, 0, 0, opt, img)));

            Assert::AreEqual (1280 * 576 / 160, img.width);    // 4608
            Assert::AreEqual (1 * 576 / 144, img.height);      // 4
        }


        TEST_METHOD (OutputDimensionsAt288)
        {
            PrintRaster              raster;
            PaperRenderer            renderer;
            PaperRenderer::Options   opt;
            RgbaImage                img;

            opt.outputDpi = 288;
            Assert::IsTrue (SUCCEEDED (renderer.Render (raster, 0, 143, opt, img)));

            Assert::AreEqual (1280 * 288 / 160, img.width);    // 2304
            Assert::AreEqual (144 * 288 / 144, img.height);    // 288
        }


        TEST_METHOD (EmptyPaperStaysWhite)
        {
            PrintRaster              raster;
            PaperRenderer            renderer;
            PaperRenderer::Options   opt;
            RgbaImage                img;
            int                      minX = 0, minY = 0, maxX = 0, maxY = 0;

            Assert::IsTrue (SUCCEEDED (renderer.Render (raster, 0, 3, opt, img)));

            InkBounds (img, minX, minY, maxX, maxY);
            Assert::AreEqual (-1, maxX);                        // nothing inked
        }


        TEST_METHOD (EmptyRangeProducesEmptyImage)
        {
            PrintRaster              raster;
            PaperRenderer            renderer;
            PaperRenderer::Options   opt;
            RgbaImage                img;

            Assert::IsTrue (SUCCEEDED (renderer.Render (raster, 10, 5, opt, img)));

            Assert::AreEqual (0, img.width);
            Assert::AreEqual (0, img.height);
        }


        TEST_METHOD (SingleDotIsInkedAndRound)
        {
            PrintRaster              raster;
            PaperRenderer            renderer;
            PaperRenderer::Options   opt;
            RgbaImage                img;
            int                      cx = 0, cy = 0;

            raster.Strike (100, 20, InkPrimary::Black);
            opt.outputDpi = 576;

            // Render a band tall enough to contain the whole dot.
            Assert::IsTrue (SUCCEEDED (renderer.Render (raster, 16, 24, opt, img)));

            CellCenter (100, 20 - 16, 576, cx, cy);
            Assert::IsTrue (IsInked (img, cx, cy));            // centre struck

            // A pixel well outside the ~8px dot is clean paper.
            Assert::IsFalse (IsInked (img, cx + 20, cy));
        }


        TEST_METHOD (DeterministicAcrossRuns)
        {
            PrintRaster              raster;
            PaperRenderer            r1;
            PaperRenderer            r2;
            PaperRenderer::Options   opt;
            RgbaImage                a;
            RgbaImage                b;
            int                      i = 0;

            raster.Strike (10, 0, InkPrimary::Red);
            raster.Strike (11, 0, InkPrimary::Blue);
            raster.Strike (12, 1, InkPrimary::Yellow);

            Assert::IsTrue (SUCCEEDED (r1.Render (raster, 0, 3, opt, a)));
            Assert::IsTrue (SUCCEEDED (r2.Render (raster, 0, 3, opt, b)));

            Assert::AreEqual (a.rgba.size(), b.rgba.size());
            for (i = 0; i < (int) a.rgba.size(); i++)
            {
                Assert::AreEqual (a.rgba[i], b.rgba[i]);
            }
        }


        TEST_METHOD (CompositeColoursFollowOverprint)
        {
            PrintRaster              raster;
            PaperRenderer            renderer;
            PaperRenderer::Options   opt;
            RgbaImage                img;
            int                      cx = 0, cy = 0;

            opt.outputDpi = 576;
            raster.Strike (0,  0, InkPrimary::Yellow);                     // yellow
            raster.Strike (10, 0, InkPrimary::Yellow);
            raster.Strike (10, 0, InkPrimary::Red);                        // orange
            raster.Strike (20, 0, InkPrimary::Yellow);
            raster.Strike (20, 0, InkPrimary::Blue);                       // green
            raster.Strike (30, 0, InkPrimary::Red);
            raster.Strike (30, 0, InkPrimary::Blue);                       // purple

            Assert::IsTrue (SUCCEEDED (renderer.Render (raster, 0, 0, opt, img)));

            {
                const Byte *   yellow = nullptr;
                const Byte *   orange = nullptr;
                const Byte *   green  = nullptr;
                const Byte *   purple = nullptr;

                CellCenter (0, 0, 576, cx, cy);   yellow = img.PixelAt (cx, cy);
                CellCenter (10, 0, 576, cx, cy);  orange = img.PixelAt (cx, cy);
                CellCenter (20, 0, 576, cx, cy);  green  = img.PixelAt (cx, cy);
                CellCenter (30, 0, 576, cx, cy);  purple = img.PixelAt (cx, cy);

                Assert::IsTrue (yellow[0] > yellow[1] && yellow[1] > yellow[2]);   // R>G>B
                Assert::IsTrue (orange[0] > orange[1] && orange[1] > orange[2]);   // R>G>B
                Assert::IsTrue (orange[1] < yellow[1]);                            // less green than yellow
                Assert::IsTrue (green[1] > green[0] && green[1] > green[2]);       // G dominant
                Assert::IsTrue (purple[2] > purple[1] && purple[0] > purple[1]);   // B,R over G
            }
        }


        TEST_METHOD (SquarePixelGeometryIsIsotropic)
        {
            PaperRenderer            renderer;
            PaperRenderer::Options   opt;
            int                      col     = 0;
            int                      inkW    = 0;
            int                      inkH    = 0;

            opt.outputDpi = 576;

            // One inch horizontally: 160 cells across a single row.
            {
                PrintRaster   raster;
                RgbaImage     img;
                int           minX = 0, minY = 0, maxX = 0, maxY = 0;

                for (col = 0; col < 160; col++)
                {
                    raster.Strike (col, 0, InkPrimary::Black);
                }
                Assert::IsTrue (SUCCEEDED (renderer.Render (raster, 0, 0, opt, img)));
                InkBounds (img, minX, minY, maxX, maxY);
                inkW = maxX - minX + 1;
            }

            // One inch vertically: 144 rows in a single column.
            {
                PrintRaster   raster;
                RgbaImage     img;
                int           minX = 0, minY = 0, maxX = 0, maxY = 0;
                int           row  = 0;

                for (row = 0; row < 144; row++)
                {
                    raster.Strike (0, row, InkPrimary::Black);
                }
                Assert::IsTrue (SUCCEEDED (renderer.Render (raster, 0, 143, opt, img)));
                InkBounds (img, minX, minY, maxX, maxY);
                inkH = maxY - minY + 1;
            }

            // SC-009: a physical inch measures equal within 1% on both axes.
            {
                int   bigger = inkW > inkH ? inkW : inkH;
                int   diff   = inkW > inkH ? inkW - inkH : inkH - inkW;
                Assert::IsTrue (diff * 100 <= bigger, L"H/V extents differ by more than 1%");
            }
        }


        TEST_METHOD (PlainStyleFillsCell)
        {
            PrintRaster              raster;
            PaperRenderer            renderer;
            PaperRenderer::Options   opt;
            RgbaImage                img;

            opt.outputDpi = 576;
            opt.style     = DotStyle::Plain;
            raster.Strike (50, 0, InkPrimary::Black);

            Assert::IsTrue (SUCCEEDED (renderer.Render (raster, 0, 0, opt, img)));

            // The cell's top-left output pixel is filled (square, not a disc).
            {
                int   x0 = 50 * 576 / 160;
                Assert::IsTrue (IsInked (img, x0, 0));
            }
        }
    };
}
