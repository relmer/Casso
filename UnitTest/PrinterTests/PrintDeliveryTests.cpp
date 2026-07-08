#include "Pch.h"

#include "Devices/Printer/PrintDelivery.h"
#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/PrintRaster.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;




////////////////////////////////////////////////////////////////////////////////
//
//  PrintDeliveryTests
//
//  The render->encode composition: a struck strip becomes a decodable PNG of
//  the expected geometry with real ink. In-memory only (Test Isolation).
//
////////////////////////////////////////////////////////////////////////////////

namespace PrintDeliveryTests
{
    TEST_CLASS (PrintDeliveryTests)
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


        TEST_METHOD (RendersStrikesToDecodablePng)
        {
            PrintRaster    raster;
            vector<Byte>   png;
            RgbaImage      decoded;
            int            col   = 0;
            int            inked = 0;
            int            x = 0, y = 0;

            for (col = 0; col < 40; col++)
            {
                raster.Strike (col, 0, InkPrimary::Black);
                raster.Strike (col, 1, InkPrimary::Red);
            }

            Assert::IsTrue (SUCCEEDED (PrintDelivery::RenderToPng (
                raster, 0, raster.RowsUsed () - 1, 288, DotStyle::Ink, png)));
            Assert::IsTrue (png.size () > 8);

            Assert::IsTrue (SUCCEEDED (PngCodec::DecodeRgba (png, decoded)));
            Assert::AreEqual (1280 * 288 / 160, decoded.width);          // full printable width

            for (y = 0; y < decoded.height; y++)
            {
                for (x = 0; x < decoded.width; x++)
                {
                    const Byte *   p = decoded.PixelAt (x, y);
                    if (p[0] < 250 || p[1] < 250 || p[2] < 250) inked++;
                }
            }
            Assert::IsTrue (inked > 0, L"delivered PNG has no ink");
        }


    private:
        bool   m_ownsCom = false;
    };
}
