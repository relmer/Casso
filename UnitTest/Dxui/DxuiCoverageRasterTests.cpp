#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCoverageRasterTests
//
//  The CPU rasterizer behind the drop guides: a pixel wholly inside a fill
//  takes its color, a pixel outside keeps what was there, and a pixel an
//  edge crosses takes a share of the color as large as the share of its
//  samples inside, laid over the pixel in premultiplied form. Whole pictures
//  are laid over one another at an opacity, faded, and tinted only where
//  they cover.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiCoverageRasterTests
{
    static uint32_t GetPixel (const DxuiIconImage & image, int x, int y)
    {
        return image.bgraPremul[(size_t) y * (size_t) image.width + (size_t) x];
    }



    TEST_CLASS (DxuiCoverageRasterTests)
    {
    public:

        //  A right triangle with its hypotenuse at 45 degrees through the
        //  corners of the pixels along it.
        TEST_METHOD (InsideIsFullOutsideIsZeroAndADiagonalEdgeIsHalf)
        {
            DxuiIconImage            image    = DxuiCoverageRaster::MakeImage (10, 10);
            std::vector<DxuiPointF>  triangle = { DxuiPointF { 0.0f, 0.0f }, DxuiPointF { 10.0f, 0.0f }, DxuiPointF { 0.0f, 10.0f } };
            uint32_t                 edge     = 0;



            DxuiCoverageRaster::FillPolygon (image, triangle, 0xFFFFFFFFu);
            edge = GetPixel (image, 4, 5);

            Assert::AreEqual (0xFFFFFFFFu, GetPixel (image, 1, 1), L"a pixel inside is the color");
            Assert::AreEqual (0u,          GetPixel (image, 8, 8), L"a pixel outside is untouched");
            Assert::AreEqual (0x80u,       edge >> 24,             L"a pixel the diagonal halves is half covered");
            Assert::AreEqual (edge >> 24,  edge & 0xFFu,           L"in premultiplied form");
        }


        //  An edge at a quarter pixel covers a quarter of the pixel it cuts.
        TEST_METHOD (AStraightEdgeCoversItsShareOfThePixel)
        {
            DxuiIconImage     image = DxuiCoverageRaster::MakeImage (4, 4);
            DxuiCoverageRect  rect  = { 1.0f, 1.0f, 3.25f, 3.0f, 0.0f, 0.0f };



            DxuiCoverageRaster::FillRoundedRect (image, rect, 0xFF000000u);

            Assert::AreEqual (0xFF000000u, GetPixel (image, 2, 1));
            Assert::AreEqual (0x40u,       GetPixel (image, 3, 1) >> 24, L"a quarter of the pixel");
            Assert::AreEqual (0u,          GetPixel (image, 0, 1));
        }


        TEST_METHOD (AColorIsLaidOverWhatIsThere)
        {
            DxuiIconImage     image = DxuiCoverageRaster::MakeImage (2, 2);
            DxuiCoverageRect  all   = { 0.0f, 0.0f, 2.0f, 2.0f, 0.0f, 0.0f };



            DxuiCoverageRaster::FillRoundedRect (image, all, 0x80FF0000u);
            Assert::AreEqual (0x80800000u, GetPixel (image, 0, 0), L"half-transparent red, premultiplied");

            DxuiCoverageRaster::FillRoundedRect (image, all, 0xFF0000FFu);
            Assert::AreEqual (0xFF0000FFu, GetPixel (image, 0, 0), L"an opaque color covers it");
        }


        //  A ring is the outer rectangle less the inner one, and a rounded corner
        //  leaves the pixel at its very corner clear.
        TEST_METHOD (ARingLeavesItsInsideClear)
        {
            DxuiIconImage     image = DxuiCoverageRaster::MakeImage (12, 12);
            DxuiCoverageRect  outer = { 0.0f, 0.0f, 12.0f, 12.0f, 4.0f, 4.0f };
            DxuiCoverageRect  inner = { 1.0f, 1.0f, 11.0f, 11.0f, 3.0f, 3.0f };



            DxuiCoverageRaster::FillRoundedRing (image, outer, inner, 0xFFFFFFFFu);

            Assert::AreEqual (0xFFFFFFFFu, GetPixel (image, 6, 0), L"the ring's top");
            Assert::AreEqual (0xFFFFFFFFu, GetPixel (image, 0, 6), L"its side");
            Assert::AreEqual (0u,          GetPixel (image, 6, 6), L"its inside");
            Assert::AreEqual (0u,          GetPixel (image, 0, 0), L"the rounded corner");
        }


        //  Each edge of an inset polygon lies the distance inside its own,
        //  a 45-degree edge included.
        TEST_METHOD (AnInsetPolygonKeepsItsEdgesParallel)
        {
            std::vector<DxuiPointF>  outline = { DxuiPointF { 0.0f, 0.0f }, DxuiPointF { 10.0f, 0.0f }, DxuiPointF { 10.0f, 5.0f },
                                                 DxuiPointF { 5.0f, 10.0f }, DxuiPointF { 0.0f, 10.0f } };
            std::vector<DxuiPointF>  inset   = DxuiCoverageRaster::InsetPolygon (outline, 1.0f);



            Assert::AreEqual ((size_t) 5, inset.size());
            Assert::AreEqual (1.0f, inset[0].x, 0.001f, L"the top left corner moves in along both edges");
            Assert::AreEqual (1.0f, inset[0].y, 0.001f);
            Assert::AreEqual (9.0f, inset[1].x, 0.001f, L"the right side moves in one unit");
            Assert::AreEqual (9.0f, inset[2].x, 0.001f);
            Assert::AreEqual (9.0f, inset[3].y, 0.001f, L"the bottom moves up one unit");
            Assert::AreEqual (inset[2].x - inset[3].x, inset[3].y - inset[2].y, 0.001f, L"the diagonal stays at 45 degrees");
            Assert::AreEqual (1.0f, (15.0f - (inset[2].x + inset[2].y)) / std::sqrt (2.0f), 0.001f, L"one unit inside the diagonal");
            Assert::IsTrue   (DxuiCoverageRaster::IsInsidePolygon (outline, 9.5f, 0.5f));
            Assert::IsFalse  (DxuiCoverageRaster::IsInsidePolygon (inset,   9.5f, 0.5f));
        }


        //  A layer laid over a picture at 75%: where the layer is opaque, 75%
        //  of it and 25% of what was there; where it is clear, the picture.
        TEST_METHOD (ALayerIsLaidOverAtItsOpacity)
        {
            DxuiIconImage     image  = DxuiCoverageRaster::MakeImage (2, 1);
            DxuiIconImage     layer  = DxuiCoverageRaster::MakeImage (2, 1);
            DxuiIconImage     narrow = DxuiCoverageRaster::MakeImage (1, 1);
            DxuiCoverageRect  all    = { 0.0f, 0.0f, 2.0f, 1.0f, 0.0f, 0.0f };
            DxuiCoverageRect  left   = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };



            DxuiCoverageRaster::FillRoundedRect (image, all,  0xFF0000FFu);
            DxuiCoverageRaster::FillRoundedRect (layer, left, 0xFFFF0000u);
            DxuiCoverageRaster::DrawImage       (image, layer, 0.75f);

            Assert::AreEqual (0xFFBF0040u, GetPixel (image, 0, 0), L"75% red over blue");
            Assert::AreEqual (0xFF0000FFu, GetPixel (image, 1, 0), L"a clear layer pixel leaves the picture");

            DxuiCoverageRaster::DrawImage (image, narrow, 1.0f);
            Assert::AreEqual (0xFFBF0040u, GetPixel (image, 0, 0), L"a layer of another size draws nothing");
        }


        //  Fading scales every channel of a premultiplied pixel, so the
        //  picture draws as if its alpha had been scaled.
        TEST_METHOD (AFadedPictureDrawsAtTheOpacity)
        {
            DxuiIconImage     image = DxuiCoverageRaster::MakeImage (1, 1);
            DxuiCoverageRect  all   = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };



            DxuiCoverageRaster::FillRoundedRect (image, all, 0xFF80FF40u);
            DxuiCoverageRaster::FadeImage       (image, (float) DxuiDockGuide::kRestAlpha / 255.0f);

            Assert::AreEqual (0xB35AB32Du, GetPixel (image, 0, 0), L"every channel at 0xB3 / 0xFF");
        }


        //  A tint is laid over what a picture covers and nowhere else: an
        //  opaque pixel takes the tint as if the tint lay above it, a clear
        //  one stays clear, and a half-covered one takes half the tint's
        //  color. Every alpha is left as it was.
        TEST_METHOD (ATintIsLaidOnlyOverWhatThePictureCovers)
        {
            DxuiIconImage     image = DxuiCoverageRaster::MakeImage (4, 1);
            DxuiCoverageRect  first = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f };
            DxuiCoverageRect  third = { 2.0f, 0.0f, 3.0f, 1.0f, 0.0f, 0.0f };
            DxuiCoverageRect  shade = { 0.0f, 0.0f, 3.0f, 1.0f, 0.0f, 0.0f };



            DxuiCoverageRaster::FillRoundedRect (image, first, 0xFFFFFFFFu);
            DxuiCoverageRaster::FillRoundedRect (image, third, 0x80FFFFFFu);
            image.bgraPremul[3] = 0xFFFFFFFFu;

            DxuiCoverageRaster::TintCovered (image, shade, 0x800000FFu);

            Assert::AreEqual (0xFF7F7FFFu, GetPixel (image, 0, 0), L"half-alpha blue over white");
            Assert::AreEqual (0u,          GetPixel (image, 1, 0), L"a clear pixel stays clear");
            Assert::AreEqual (0x80404080u, GetPixel (image, 2, 0), L"half the tint's color on a half-covered pixel");
            Assert::AreEqual (0xFFFFFFFFu, GetPixel (image, 3, 0), L"outside the rectangle, untouched");
        }
    };
}
