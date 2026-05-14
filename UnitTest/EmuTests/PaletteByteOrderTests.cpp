#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Video/NtscColorTable.h"
#include "Video/PixelFormat.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace Casso::Video;





////////////////////////////////////////////////////////////////////////////////
//
//  PaletteByteOrderTests
//
//  Locks down the convention that the framebuffer pixel format is
//  B8G8R8A8 (DXGI_FORMAT_B8G8R8A8_UNORM, matching every Windows pixel
//  surface). Each named palette color is decomposed into its B/G/R/A
//  bytes via the helpers in Video/PixelFormat.h and asserted against
//  the human-documented RGB values from the file's comment block.
//
//  These tests catch:
//    * Hand-typed palette literals that swap two nibbles by accident
//      (the classic "is 0xFFFD44FF violet or some weird coral?" bug
//      that bit us twice).
//    * Anyone "fixing" a palette literal by swapping R/B without
//      flipping the surface format in the same change.
//    * Drift between the LoRes / DHGR tables (which are duplicated
//      identically by design).
//
//  If you ever switch the framebuffer format, you must update both
//  Video/PixelFormat.h and the palette literals, and the test will
//  still hold because the helpers ExtractR/ExtractG/ExtractB encode
//  the format convention.
//
////////////////////////////////////////////////////////////////////////////////

namespace
{
    void AssertPixelRGB (uint32_t pixel, uint8_t r, uint8_t g, uint8_t b,
                         const wchar_t * name)
    {
        wchar_t msg[256];
        swprintf_s (msg, 256,
                    L"%ls: expected R=%u G=%u B=%u, got R=%u G=%u B=%u "
                    L"(pixel = 0x%08X)",
                    name, r, g, b,
                    ExtractR (pixel), ExtractG (pixel), ExtractB (pixel),
                    pixel);

        Assert::AreEqual (r, ExtractR (pixel), msg);
        Assert::AreEqual (g, ExtractG (pixel), msg);
        Assert::AreEqual (b, ExtractB (pixel), msg);
        Assert::AreEqual (uint8_t (0xFF), ExtractA (pixel),
                          L"Alpha channel must be fully opaque");
    }
}





TEST_CLASS (PixelFormatTests)
{
public:

    TEST_METHOD (Extract_DecomposesBgraLiteralCorrectly)
    {
        // 0xAARRGGBB in B8G8R8A8 layout: byte 0 = B, byte 2 = R.
        uint32_t  pixel = 0xFFCC5511;

        Assert::AreEqual (uint8_t (0x11), ExtractB (pixel), L"B in byte 0");
        Assert::AreEqual (uint8_t (0x55), ExtractG (pixel), L"G in byte 1");
        Assert::AreEqual (uint8_t (0xCC), ExtractR (pixel), L"R in byte 2");
        Assert::AreEqual (uint8_t (0xFF), ExtractA (pixel), L"A in byte 3");
    }

    TEST_METHOD (MakePixel_RoundTripsThroughExtract)
    {
        constexpr uint32_t  pixel = MakePixel (0x12, 0x34, 0x56, 0x78);

        Assert::AreEqual (uint8_t (0x12), ExtractR (pixel));
        Assert::AreEqual (uint8_t (0x34), ExtractG (pixel));
        Assert::AreEqual (uint8_t (0x56), ExtractB (pixel));
        Assert::AreEqual (uint8_t (0x78), ExtractA (pixel));
    }

    TEST_METHOD (MakePixel_PureRedIsByte2_NotByte0)
    {
        // Sanity check that catches an accidental flip back to RGBA:
        // pure red must put 0xFF in byte 2 of memory (R position) and
        // 0x00 in byte 0 (B position). Under RGBA this would be
        // inverted, so this single assert pins the format.
        constexpr uint32_t  pixel = MakePixel (0xFF, 0x00, 0x00);
        const     Byte    * bytes = reinterpret_cast<const Byte *> (&pixel);

        Assert::AreEqual (Byte (0x00), bytes[0], L"B byte must be 0 for pure red");
        Assert::AreEqual (Byte (0xFF), bytes[2], L"R byte must be 0xFF for pure red");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  NtscPaletteByteOrderTests
//
//  Pins the six HGR-mode NTSC colors. The values here are the
//  human-readable R,G,B intent from the comment block in
//  NtscColorTable.h — if any palette literal is mistyped, the test
//  reports which channel is wrong and what it actually was.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (NtscPaletteByteOrderTests)
{
public:

    TEST_METHOD (Black_IsAllZero)
    {
        AssertPixelRGB (kNtscBlack, 0, 0, 0, L"kNtscBlack");
    }

    TEST_METHOD (White_IsAllMax)
    {
        AssertPixelRGB (kNtscWhite, 255, 255, 255, L"kNtscWhite");
    }

    TEST_METHOD (Violet_IsMagentaLeaningRed)
    {
        // RGB(255, 68, 253) — even col, palette 0
        AssertPixelRGB (kNtscViolet, 255, 68, 253, L"kNtscViolet");
    }

    TEST_METHOD (Green_IsBrightGreen)
    {
        // RGB(20, 245, 60) — odd col, palette 0
        AssertPixelRGB (kNtscGreen, 20, 245, 60, L"kNtscGreen");
    }

    TEST_METHOD (Blue_IsTrulyBlue)
    {
        // RGB(20, 207, 255) — even col, palette 1
        // This is the one that exposed the R/B-swap bug twice. If
        // this test fails with R=255 instead of B=255, somebody
        // flipped the surface format without updating the literal
        // (or vice versa).
        AssertPixelRGB (kNtscBlue, 20, 207, 255, L"kNtscBlue");
    }

    TEST_METHOD (Orange_IsTrulyOrange)
    {
        // RGB(255, 106, 60) — odd col, palette 1
        AssertPixelRGB (kNtscOrange, 255, 106, 60, L"kNtscOrange");
    }
};
