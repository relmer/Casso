#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Video/NtscColorTable.h"
#include "Video/PixelFormat.h"
#include "Video/MonochromeTint.h"

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





////////////////////////////////////////////////////////////////////////////////
//
//  MonochromeTintTests
//
//  Pins the View menu's three monochrome modes. After the framebuffer
//  format switched from RGBA to BGRA, the tint code in EmulatorShell
//  was reading R/G/B from the wrong byte positions AND reconstructing
//  in the wrong order, which made Amber render as a cyan-ish blue.
//  These tests use the helpers in Video/MonochromeTint.h, which
//  EmulatorShell now also calls — so anyone who breaks the BGRA
//  arithmetic again will fail here before the user sees it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MonochromeTintTests)
{
public:

    TEST_METHOD (Luminance_PureRed_IsRec601Weight)
    {
        // R=255, G=0, B=0 -> 0.299 * 255 = 76.245 -> 76
        Assert::AreEqual (uint8_t (76),
            Casso::Video::Luminance (Casso::Video::MakePixel (255, 0, 0)),
            L"Pure red should yield Rec.601 luma 76");
    }

    TEST_METHOD (Luminance_PureGreen_IsRec601Weight)
    {
        // R=0, G=255, B=0 -> 0.587 * 255 = 149.685 -> 149
        Assert::AreEqual (uint8_t (149),
            Casso::Video::Luminance (Casso::Video::MakePixel (0, 255, 0)),
            L"Pure green should yield Rec.601 luma 149");
    }

    TEST_METHOD (Luminance_PureBlue_IsRec601Weight)
    {
        // R=0, G=0, B=255 -> 0.114 * 255 = 29.07 -> 29
        Assert::AreEqual (uint8_t (29),
            Casso::Video::Luminance (Casso::Video::MakePixel (0, 0, 255)),
            L"Pure blue should yield Rec.601 luma 29");
    }

    TEST_METHOD (TintGreenMono_OnWhite_GivesPureGreen)
    {
        uint32_t  out = Casso::Video::TintGreenMono (
            Casso::Video::MakePixel (255, 255, 255));

        Assert::AreEqual (uint8_t (0),   Casso::Video::ExtractR (out),
            L"Green mono must zero R");
        Assert::AreEqual (uint8_t (255), Casso::Video::ExtractG (out),
            L"Green mono of white must put luma in G");
        Assert::AreEqual (uint8_t (0),   Casso::Video::ExtractB (out),
            L"Green mono must zero B");
    }

    TEST_METHOD (TintAmberMono_OnWhite_GivesAmber_NotBlue)
    {
        // Amber is R=L, G=L*0.75, B=0. The bug under the old RGBA-in-
        // BGRA code wrote B=L instead of R=L, producing a cyan-blue
        // pixel. This assertion would have caught that on day one.
        uint32_t  out = Casso::Video::TintAmberMono (
            Casso::Video::MakePixel (255, 255, 255));

        Assert::AreEqual (uint8_t (255), Casso::Video::ExtractR (out),
            L"Amber mono of white must put full luma in R "
            L"(not B — that bug renders as blue on screen)");
        Assert::AreEqual (uint8_t (191), Casso::Video::ExtractG (out),
            L"Amber mono of white must put 0.75 * luma in G "
            L"(255 * 0.75 = 191.25 -> 191)");
        Assert::AreEqual (uint8_t (0),   Casso::Video::ExtractB (out),
            L"Amber mono must zero B");
    }

    TEST_METHOD (TintWhiteMono_OnRed_GivesGreyWithR_EqualsLuma)
    {
        // Pure red (luma 76) tinted white-mono should be rgb(76,76,76).
        uint32_t  out = Casso::Video::TintWhiteMono (
            Casso::Video::MakePixel (255, 0, 0));

        Assert::AreEqual (uint8_t (76), Casso::Video::ExtractR (out),
            L"White mono of red should produce rgb(76,76,76)");
        Assert::AreEqual (uint8_t (76), Casso::Video::ExtractG (out));
        Assert::AreEqual (uint8_t (76), Casso::Video::ExtractB (out));
    }

    TEST_METHOD (TintAmberMono_OnNtscBlue_PreservesAmberHue)
    {
        // The pixel that used to render as a wrong color is the one
        // that's all-blue going in. Under amber mono it should still
        // come out amber-tinted, not stay blue.
        uint32_t  out = Casso::Video::TintAmberMono (kNtscBlue);
        Assert::IsTrue (Casso::Video::ExtractR (out) > 0,
            L"Amber tint of any input must have R > 0 (no all-blue output)");
        Assert::AreEqual (uint8_t (0), Casso::Video::ExtractB (out),
            L"Amber tint must always produce B = 0");
    }
};
