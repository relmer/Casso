#include "Pch.h"

#include "Capture/CapturedImage.h"

#include "../EhmTestHelper.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  CapturedImageTests
//
//  The BGRA-to-RGBA swap, which is a channel order and therefore exactly the
//  kind of thing that is wrong for a release before anyone notices the sky is
//  orange. Distinct values per channel, so a swap in the wrong direction and a
//  swap that does nothing both fail.
//
////////////////////////////////////////////////////////////////////////////////

namespace CapturedImageTests
{
    static CapturedImage OnePixel (Byte b, Byte g, Byte r, Byte a)
    {
        CapturedImage   image;
        image.widthPx  = 1;
        image.heightPx = 1;
        image.bgra     = { b, g, r, a };
        return image;
    }




    TEST_CLASS (CapturedImageTests)
    {
    public:

        TEST_METHOD (SwapsBlueAndRedAndLeavesGreenAndAlpha)
        {
            CapturedImage   src = OnePixel (0x11, 0x22, 0x33, 0x44);
            RgbaImage       out;

            AssertSucceeded (CapturedImage::ToRgbaImage (src, out));

            Assert::AreEqual ((int) 0x33, (int) out.rgba[0], L"R comes from the third BGRA byte");
            Assert::AreEqual ((int) 0x22, (int) out.rgba[1], L"G does not move");
            Assert::AreEqual ((int) 0x11, (int) out.rgba[2], L"B comes from the first BGRA byte");
            Assert::AreEqual ((int) 0x44, (int) out.rgba[3], L"A does not move");
        }


        TEST_METHOD (CarriesDimensionsThrough)
        {
            CapturedImage   src;
            RgbaImage       out;

            src.widthPx  = 3;
            src.heightPx = 2;
            src.bgra.resize (3 * 2 * 4, 0x7F);

            AssertSucceeded (CapturedImage::ToRgbaImage (src, out));

            Assert::AreEqual (3, out.width);
            Assert::AreEqual (2, out.height);
            Assert::AreEqual ((size_t) (3 * 2 * 4), out.rgba.size());
        }


        // Row-major order must survive, or the image comes back transposed.
        TEST_METHOD (PreservesPixelOrderAcrossRows)
        {
            CapturedImage   src;
            RgbaImage       out;

            src.widthPx  = 2;
            src.heightPx = 2;
            src.bgra     = { 0x01, 0x00, 0x00, 0xFF,    // (0,0) blue-ish
                             0x00, 0x00, 0x02, 0xFF,    // (1,0) red-ish
                             0x00, 0x03, 0x00, 0xFF,    // (0,1) green-ish
                             0x04, 0x04, 0x04, 0xFF };  // (1,1) gray

            AssertSucceeded (CapturedImage::ToRgbaImage (src, out));

            Assert::AreEqual ((int) 0x01, (int) out.GetPixel (0, 0)[2]);
            Assert::AreEqual ((int) 0x02, (int) out.GetPixel (1, 0)[0]);
            Assert::AreEqual ((int) 0x03, (int) out.GetPixel (0, 1)[1]);
            Assert::AreEqual ((int) 0x04, (int) out.GetPixel (1, 1)[0]);
        }


        TEST_METHOD (IsValidRejectsEmptyAndUndersizedBuffers)
        {
            CapturedImage   empty;
            CapturedImage   short_;

            short_.widthPx  = 4;
            short_.heightPx = 4;
            short_.bgra.resize (4 * 4 * 4 - 1);

            Assert::IsFalse (empty.IsValid());
            Assert::IsFalse (short_.IsValid(), L"One byte short still reads off the end");
        }


        // A buffer larger than the dimensions need is fine: a caller that
        // reserved room and filled part of it has done nothing wrong.
        TEST_METHOD (IsValidAcceptsAnOversizedBuffer)
        {
            CapturedImage   image;

            image.widthPx  = 2;
            image.heightPx = 2;
            image.bgra.resize (2 * 2 * 4 + 64);

            Assert::IsTrue (image.IsValid());
        }


        // Handing over an invalid image is a caller bug rather than bad data,
        // so the rejection asserts in production. The guard proves it both
        // fires and returns gracefully -- and that the validation has not
        // quietly rotted away.
        TEST_METHOD (ConversionRefusesAnInvalidImage)
        {
            CapturedImage   image;
            RgbaImage       out;
            HRESULT         hr = S_OK;

            {
                UnitTestHelpers::ExpectedEhmAssert   expect;

                hr = CapturedImage::ToRgbaImage (image, out);
            }

            Assert::IsTrue (FAILED (hr));
        }
    };
}
