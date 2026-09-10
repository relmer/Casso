#include "Pch.h"

#include "Capture/CapturedImage.h"
#include "Capture/ScreenshotPlan.h"
#include "Devices/Printer/PngCodec.h"
#include "Devices/Printer/RgbaImage.h"
#include "Shell/ScreenshotCapture.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ScreenshotCaptureTests
//
//  The Raw capture path from a framebuffer to a PNG and back, with no window,
//  no device and no file.
//
//  The framebuffer here is synthetic: every pixel encodes its own position,
//  so a byte order slip, a stride mistake or a row dropped off the bottom
//  shows up as a specific wrong pixel rather than a picture that merely looks
//  a bit off. The round trip is the whole assertion -- what went in comes back
//  out, at the size it went in at.
//
////////////////////////////////////////////////////////////////////////////////

namespace ScreenshotCaptureTests
{
    TEST_CLASS (ScreenshotCaptureTests)
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
                CoUninitialize();
            }
        }


        TEST_METHOD (AFramebufferCaptureDecodesBackToTheSamePixels)
        {
            std::vector<uint32_t>       framebuffer      = MakePositionalFramebuffer();
            std::mutex                  framebufferMutex;
            ScreenshotCapture::Sources  sources;
            ScreenshotPlan              plan;
            CapturedImage               captured;
            RgbaImage                   rgba;
            RgbaImage                   decoded;
            std::vector<Byte>           png;
            HRESULT                     hr               = S_OK;

            //  Only the framebuffer is offered. A capture that reached for
            //  the window or the renderer here would dereference null, which
            //  is the point: the Raw path must need neither.
            sources.framebuffer      = framebuffer.data();
            sources.framebufferMutex = &framebufferMutex;
            sources.framebufferSize  = { kWidth, kHeight };
            plan.source              = CaptureSource::Framebuffer;

            hr = ScreenshotCapture::AcquirePixels (plan, sources, captured);
            AssertSucceeded (hr, L"acquiring from a framebuffer alone must succeed");
            Assert::AreEqual (kWidth,  captured.widthPx,  L"captured width");
            Assert::AreEqual (kHeight, captured.heightPx, L"captured height");

            hr = CapturedImage::ToRgbaImage (captured, rgba);
            AssertSucceeded (hr);

            hr = PngCodec::EncodeRgba (rgba, 0, png);
            AssertSucceeded (hr, L"encoding to PNG");
            Assert::IsFalse (png.empty(), L"the encoder must produce bytes");

            hr = PngCodec::DecodeRgba (png, decoded);
            AssertSucceeded (hr, L"decoding the PNG back");

            Assert::AreEqual (kWidth,  decoded.width,  L"decoded width");
            Assert::AreEqual (kHeight, decoded.height, L"decoded height");

            for (int y = 0; y < kHeight; y++)
            {
                for (int x = 0; x < kWidth; x++)
                {
                    uint32_t      expected = PixelAt (x, y);
                    const Byte *  actual   = decoded.rgba.data() + ((size_t) y * kWidth + x) * 4;
                    uint32_t      got      = ((uint32_t) actual[0] << 16)   // R
                                           | ((uint32_t) actual[1] <<  8)   // G
                                           | ((uint32_t) actual[2])         // B
                                           | ((uint32_t) actual[3] << 24);  // A

                    if (got != expected)
                    {
                        Assert::Fail (std::format (L"pixel ({},{}) went in as {:08X} and came back as {:08X}",
                                                   x, y, expected, got).c_str());
                    }
                }
            }
        }


    private:

        //  Small and not square, so a transposed width and height cannot pass.
        static constexpr int  kWidth  = 37;
        static constexpr int  kHeight = 23;

        bool  m_ownsCom = false;


        //  A pixel that says where it is: opaque, with x in red and y in
        //  green, and blue tied to both so no two pixels agree.
        static uint32_t PixelAt (int x, int y)
        {
            return 0xFF000000u
                 | ((uint32_t) (x * 6) << 16)
                 | ((uint32_t) (y * 11) << 8)
                 | ((uint32_t) ((x * 7 + y * 13) & 0xFF));
        }


        static std::vector<uint32_t> MakePositionalFramebuffer()
        {
            std::vector<uint32_t>  fb ((size_t) kWidth * kHeight);

            for (int y = 0; y < kHeight; y++)
            {
                for (int x = 0; x < kWidth; x++)
                {
                    fb[(size_t) y * kWidth + x] = PixelAt (x, y);
                }
            }

            return fb;
        }
    };
}
