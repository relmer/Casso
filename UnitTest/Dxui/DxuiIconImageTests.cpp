#include "Pch.h"

#include "Core/DxuiIconImage.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiIconImageTests
//
//  What an icon becomes once rasterized: premultiplied pixels, whether it
//  carries an alpha channel or only a mask. The icons are built from known
//  2x2 pixels with CreateIconIndirect, so no resource and no desktop is
//  involved.
//
//  The premultiplied case is the one that matters. GDI already writes an
//  alpha icon premultiplied, and a second multiply -- which Casso's caption
//  once applied -- darkens every partly transparent edge.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiIconImageTests)
{
public:

    //  A 2x2 icon whose color is a 32-bit DIB of the given pixels, top row
    //  first, and whose monochrome mask is the given bytes (a set bit is a
    //  transparent pixel; each row padded to a word).
    static HICON  MakeIcon (const uint32_t pixels[4], const BYTE maskBits[4])
    {
        BITMAPINFO  bmi   = {};
        void      * raw   = nullptr;
        HBITMAP     color = nullptr;
        HBITMAP     mask  = nullptr;
        ICONINFO    info  = {};
        HICON       icon  = nullptr;
        HDC         dc    = GetDC (nullptr);

        bmi.bmiHeader.biSize        = sizeof (BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth       = 2;
        bmi.bmiHeader.biHeight      = -2;
        bmi.bmiHeader.biPlanes      = 1;
        bmi.bmiHeader.biBitCount    = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        color = CreateDIBSection (dc, &bmi, DIB_RGB_COLORS, &raw, nullptr, 0);
        ReleaseDC (nullptr, dc);

        if (color == nullptr || raw == nullptr)
        {
            return nullptr;
        }

        memcpy (raw, pixels, 4 * sizeof (uint32_t));

        mask = CreateBitmap (2, 2, 1, 1, maskBits);

        info.fIcon    = TRUE;
        info.hbmColor = color;
        info.hbmMask  = mask;

        icon = CreateIconIndirect (&info);

        DeleteObject (color);
        DeleteObject (mask);

        return icon;
    }


    static bool  IsWithin (uint32_t actual, uint32_t expected, uint32_t tolerance)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            int  a = (int) ((actual   >> shift) & 0xFF);
            int  e = (int) ((expected >> shift) & 0xFF);

            if (std::abs (a - e) > (int) tolerance)
            {
                return false;
            }
        }

        return true;
    }


    TEST_METHOD (AlphaIcon_ComesOutPremultipliedOnce)
    {
        static constexpr uint32_t  kPixels[4] =
        {
            0xFFFF0000u,   //  opaque red
            0x80FFFFFFu,   //  white at half opacity, straight alpha
            0x00000000u,   //  transparent
            0xFF0000FFu,   //  opaque blue
        };
        static constexpr BYTE      kMask[4] = { 0x00, 0x00, 0x00, 0x00 };

        HICON          icon  = MakeIcon (kPixels, kMask);
        DxuiIconImage  image;
        HRESULT        hr    = E_FAIL;


        Assert::IsNotNull (icon, L"A 2x2 alpha icon can be built");

        hr = DxuiIconImage::FromHicon (icon, 2, image);
        DestroyIcon (icon);

        Assert::IsTrue   (SUCCEEDED (hr));
        Assert::AreEqual (4, (int) image.bgraPremul.size());

        Assert::AreEqual (0xFFFF0000u, image.bgraPremul[0], L"An opaque pixel is its own premultiplied color");
        Assert::IsTrue   (IsWithin (image.bgraPremul[1], 0x80808080u, 1),
            L"Half-opaque white is color times alpha once -- about 0x80 in every channel, not the 0x40 a second multiply gives");
        Assert::AreEqual (0u, image.bgraPremul[2] >> 24, L"A transparent pixel stays transparent");
        Assert::AreEqual (0xFF0000FFu, image.bgraPremul[3]);
    }


    TEST_METHOD (MaskOnlyIcon_TakesItsAlphaFromTheMask)
    {
        static constexpr uint32_t  kPixels[4] =
        {
            0x00FF0000u,   //  red, no alpha channel
            0x00000000u,
            0x00000000u,
            0x000000FFu,   //  blue, no alpha channel
        };

        //  Row 0: pixel 0 opaque, pixel 1 transparent. Row 1: pixel 2
        //  transparent, pixel 3 opaque. Most significant bit first.
        static constexpr BYTE      kMask[4] = { 0x40, 0x00, 0x80, 0x00 };

        HICON          icon  = MakeIcon (kPixels, kMask);
        DxuiIconImage  image;
        HRESULT        hr    = E_FAIL;


        Assert::IsNotNull (icon, L"A 2x2 mask-only icon can be built");

        hr = DxuiIconImage::FromHicon (icon, 2, image);
        DestroyIcon (icon);

        Assert::IsTrue   (SUCCEEDED (hr));
        Assert::AreEqual (0xFFFF0000u, image.bgraPremul[0], L"Where the mask keeps a pixel it is opaque");
        Assert::AreEqual (0u,          image.bgraPremul[1], L"and where it does not, nothing is left");
        Assert::AreEqual (0u,          image.bgraPremul[2]);
        Assert::AreEqual (0xFF0000FFu, image.bgraPremul[3]);
    }
};
