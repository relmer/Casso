#include "Pch.h"

#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/RgbaImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  PngCodecTests
//
//  In-memory PNG round-trips over WIC: RGBA fidelity (including channel order),
//  pHYs dpi survival, and lossless 8bpp index-plane preservation. All buffers
//  are in memory -- no files touched -- so this runs anywhere per Test Isolation.
//
////////////////////////////////////////////////////////////////////////////////

namespace PngCodecTests
{
    TEST_CLASS (PngCodecTests)
    {
    public:

        TEST_METHOD_INITIALIZE (InitCom)
        {
            HRESULT   hr = CoInitializeEx (nullptr, COINIT_APARTMENTTHREADED);
            m_ownsCom = (hr == S_OK || hr == S_FALSE);
        }


        TEST_METHOD_CLEANUP (UninitCom)
        {
            if (m_ownsCom)
            {
                CoUninitialize ();
            }
        }


        TEST_METHOD (RgbaRoundTripPreservesPixels)
        {
            RgbaImage      src;
            RgbaImage      back;
            vector<Byte>   png;

            // 2x2 with channel-distinct colours so a R<->B swap would show.
            src.Allocate (2, 2, 0, 0, 0);
            SetPixel (src, 0, 0, 0xFF, 0x00, 0x00, 0xFF);   // red
            SetPixel (src, 1, 0, 0x00, 0x00, 0xFF, 0xFF);   // blue
            SetPixel (src, 0, 1, 0x10, 0x80, 0xF0, 0xFF);   // R<G<B gradient
            SetPixel (src, 1, 1, 0x20, 0x40, 0x60, 0x80);   // partial alpha

            Assert::IsTrue (SUCCEEDED (PngCodec::EncodeRgba (src, 576, png)));
            Assert::IsTrue (png.size () > 8);
            Assert::IsTrue (SUCCEEDED (PngCodec::DecodeRgba (png, back)));

            Assert::AreEqual (src.width,  back.width);
            Assert::AreEqual (src.height, back.height);
            for (size_t i = 0; i < src.rgba.size (); i++)
            {
                Assert::AreEqual (src.rgba[i], back.rgba[i]);
            }
        }


        TEST_METHOD (PhysDpiSurvives)
        {
            RgbaImage      src;
            vector<Byte>   png;
            int            dpi = 0;

            src.Allocate (4, 4, 0xFF, 0xFF, 0xFF);
            Assert::IsTrue (SUCCEEDED (PngCodec::EncodeRgba (src, 576, png)));
            Assert::IsTrue (SUCCEEDED (PngCodec::ReadDpi (png, dpi)));

            Assert::AreEqual (576, dpi);
        }


        TEST_METHOD (IndexPlaneRoundTripIsLossless)
        {
            vector<Byte>   indices;
            vector<Byte>   back;
            vector<Byte>   png;
            uint32_t       palette[16];
            int            w = 4;
            int            h = 4;
            int            i = 0;
            int            outW = 0;
            int            outH = 0;

            for (i = 0; i < 16; i++)
            {
                palette[i] = 0xFF000000u | (uint32_t) (i * 0x101010);   // opaque grey ramp
                indices.push_back ((Byte) i);                          // one cell per index 0..15
            }

            Assert::IsTrue (SUCCEEDED (
                PngCodec::EncodeIndexed (w, h, indices, palette, 16, 576, png)));
            Assert::IsTrue (SUCCEEDED (
                PngCodec::DecodeIndexed (png, outW, outH, back)));

            Assert::AreEqual (w, outW);
            Assert::AreEqual (h, outH);
            Assert::AreEqual (indices.size (), back.size ());
            for (i = 0; i < (int) indices.size (); i++)
            {
                Assert::AreEqual (indices[i], back[i]);
            }
        }


        TEST_METHOD (EmptyInputRejected)
        {
            vector<Byte>   empty;
            RgbaImage      img;

            Assert::IsFalse (SUCCEEDED (PngCodec::DecodeRgba (empty, img)));
        }


    private:
        bool   m_ownsCom = false;

        static void SetPixel (RgbaImage & img, int x, int y, Byte r, Byte g, Byte b, Byte a)
        {
            Byte *   p = img.PixelAt (x, y);
            p[0] = r;
            p[1] = g;
            p[2] = b;
            p[3] = a;
        }
    };
}
