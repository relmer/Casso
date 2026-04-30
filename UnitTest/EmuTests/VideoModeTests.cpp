#include "../Casso65EmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/RamDevice.h"
#include "Video/AppleTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextModeTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleTextModeTests)
{
public:

    TEST_METHOD (Render_Row0Col0_NormalSpace_AllBlack)
    {
        // Normal space character ($A0) at row 0 col 0 should produce
        // black pixels in the first character cell
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        // Write normal space ($A0) at text page 1 row 0, col 0 = $0400
        bus.WriteByte (0x0400, 0xA0);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        // Framebuffer: 560x384 (40*7*2 x 24*8*2)
        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data (), fbW, fbH);

        // Normal space should render all black in first cell (14x16 pixels)
        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 14; x++)
            {
                Assert::AreEqual (0xFF000000u, fb[y * fbW + x],
                    L"Normal space should render black pixels");
            }
        }
    }

    TEST_METHOD (Render_InverseChar_HasGreenPixels)
    {
        // Inverse 'A' ($01) should render with some green (inverted) pixels
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        // Inverse 'A' = $01 at row 0 col 0
        bus.WriteByte (0x0400, 0x01);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data (), fbW, fbH);

        // Inverse character should have some green pixels (inverted rendering)
        bool hasGreen = false;

        for (int y = 0; y < 16 && !hasGreen; y++)
        {
            for (int x = 0; x < 14 && !hasGreen; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u)
                {
                    hasGreen = true;
                }
            }
        }

        Assert::IsTrue (hasGreen, L"Inverse character should have green pixels");
    }

    TEST_METHOD (Render_NormalA_HasGreenPixels)
    {
        // Normal 'A' = $C1 at row 0 col 0
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0xC1);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data (), fbW, fbH);

        // Normal 'A' glyph should have some green pixels
        bool hasGreen = false;

        for (int y = 0; y < 16 && !hasGreen; y++)
        {
            for (int x = 0; x < 14 && !hasGreen; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u)
                {
                    hasGreen = true;
                }
            }
        }

        Assert::IsTrue (hasGreen, L"Normal 'A' character should have green pixels");
    }

    TEST_METHOD (Render_FlashChar_AlternatesAcrossFrames)
    {
        // Flash 'A' = $41 should alternate between inverse and normal.
        // Render() increments an internal frame counter and toggles flash
        // every 16 frames, so we render 17 times to capture both states.
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0x41);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb1 (fbW * fbH, 0);
        std::vector<uint32_t> fb2 (fbW * fbH, 0);

        // First render (frame 1, flash on = true -> inverse)
        textMode.Render (nullptr, fb1.data (), fbW, fbH);

        // Render 16 more times to toggle flash state
        for (int i = 0; i < 16; i++)
        {
            textMode.Render (nullptr, fb2.data (), fbW, fbH);
        }

        // The first and last renders should differ since flash toggled
        bool differs = false;

        for (int y = 0; y < 16 && !differs; y++)
        {
            for (int x = 0; x < 14 && !differs; x++)
            {
                if (fb1[y * fbW + x] != fb2[y * fbW + x])
                {
                    differs = true;
                }
            }
        }

        Assert::IsTrue (differs, L"Flash character should differ across frame toggle boundary");
    }

    TEST_METHOD (Render_Row8_UsesInterleavedAddress)
    {
        // Row 8 text address = $0400 + 128*(8%8) + 40*(8/8) = $0400 + 0 + 40 = $0428
        // Writing a visible char there should produce pixels in row 8's cell
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        // Fill all with spaces
        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            bus.WriteByte (addr, 0xA0);
        }

        // Normal 'A' at row 8 col 0 = address $0428
        bus.WriteByte (0x0428, 0xC1);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data (), fbW, fbH);

        // Row 8 starts at fbY = 8*8*2 = 128
        bool hasGreen = false;

        for (int y = 128; y < 144 && !hasGreen; y++)
        {
            for (int x = 0; x < 14 && !hasGreen; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u)
                {
                    hasGreen = true;
                }
            }
        }

        Assert::IsTrue (hasGreen, L"Row 8 should render character at interleaved address $0428");
    }

    TEST_METHOD (Render_Page2_ReadsFrom0800)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        // Fill page 1 with spaces
        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            bus.WriteByte (addr, 0xA0);
        }

        // Fill page 2 with spaces too
        for (Word addr = 0x0800; addr < 0x0C00; addr++)
        {
            bus.WriteByte (addr, 0xA0);
        }

        // Write normal 'A' at page 2 row 0 col 0 = $0800
        bus.WriteByte (0x0800, 0xC1);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (true);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data (), fbW, fbH);

        bool hasGreen = false;

        for (int y = 0; y < 16 && !hasGreen; y++)
        {
            for (int x = 0; x < 14 && !hasGreen; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u)
                {
                    hasGreen = true;
                }
            }
        }

        Assert::IsTrue (hasGreen, L"Page 2 text mode should read from $0800");
    }

    TEST_METHOD (GetActivePageAddress_ReturnsCorrectPages)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleTextMode textMode (bus);

        Assert::AreEqual (static_cast<Word> (0x0400), textMode.GetActivePageAddress (false));
        Assert::AreEqual (static_cast<Word> (0x0800), textMode.GetActivePageAddress (true));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleLoResModeTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleLoResModeTests)
{
public:

    TEST_METHOD (Render_NybbleDecoding_TopAndBottomColors)
    {
        // Byte $D7 = low nybble 7 (light blue), high nybble D (yellow)
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        // Fill with zeros
        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        // Write $D7 at row 0 col 0 = $0400
        bus.WriteByte (0x0400, 0xD7);

        AppleLoResMode lores (bus);
        lores.SetPage2 (false);

        // Framebuffer: 560x384 (40*14 x 48*8)
        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        lores.Render (nullptr, fb.data (), fbW, fbH);

        // Lo-res palette color 7 = Light Blue (0xFFFF4499)
        // Lo-res palette color 13 = Yellow (0xFF00FFFF)
        uint32_t expectedTop    = 0xFFFF4499u;  // color index 7
        uint32_t expectedBottom = 0xFF00FFFFu;   // color index 13

        // Block dimensions: 560/40 = 14 wide, 384/48 = 8 tall
        // Top block at lo-res row 0: fbY 0-7
        Assert::AreEqual (expectedTop, fb[0],
            L"Top block should be color index 7 (low nybble)");

        // Bottom block at lo-res row 1: fbY 8-15
        Assert::AreEqual (expectedBottom, fb[8 * fbW],
            L"Bottom block should be color index 13 (high nybble)");
    }

    TEST_METHOD (Render_Color0_IsBlack)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0x00);

        AppleLoResMode lores (bus);
        lores.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0xFFFFFFFF);

        lores.Render (nullptr, fb.data (), fbW, fbH);

        // Color 0 = black (0xFF000000)
        Assert::AreEqual (0xFF000000u, fb[0], L"Color 0 should be black");
    }

    TEST_METHOD (Render_ColorF_IsWhite)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0xFF);

        AppleLoResMode lores (bus);
        lores.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        lores.Render (nullptr, fb.data (), fbW, fbH);

        // Color 15 = white (0xFFFFFFFF)
        Assert::AreEqual (0xFFFFFFFFu, fb[0], L"Color 15 should be white");
    }

    TEST_METHOD (GetActivePageAddress_ReturnsCorrectPages)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        AppleLoResMode lores (bus);

        Assert::AreEqual (static_cast<Word> (0x0400), lores.GetActivePageAddress (false));
        Assert::AreEqual (static_cast<Word> (0x0800), lores.GetActivePageAddress (true));
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleHiResModeTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleHiResModeTests)
{
public:

    TEST_METHOD (ScanlineAddress_Row0_ReturnsPageBase)
    {
        // Scanline 0: group=0, subRow=0, lineInGroup=0
        // = pageBase + 0 + 0 + 0
        Word addr = AppleHiResMode::ScanlineAddress (0, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2000), addr);
    }

    TEST_METHOD (ScanlineAddress_Row1_Returns128Offset)
    {
        // Scanline 1: group=0, subRow=1, lineInGroup=0
        // = 0x2000 + 1*1024 + 0 + 0 = 0x2400
        Word addr = AppleHiResMode::ScanlineAddress (1, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2400), addr);
    }

    TEST_METHOD (ScanlineAddress_Row8_ReturnsCorrectInterleave)
    {
        // Scanline 8: group=0, subRow=0, lineInGroup=1
        // = 0x2000 + 0 + 0 + 1*128 = 0x2080
        Word addr = AppleHiResMode::ScanlineAddress (8, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2080), addr);
    }

    TEST_METHOD (ScanlineAddress_Row64_SecondGroup)
    {
        // Scanline 64: group=1, subRow=0, lineInGroup=0
        // = 0x2000 + 0 + 1*40 + 0 = 0x2028
        Word addr = AppleHiResMode::ScanlineAddress (64, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2028), addr);
    }

    TEST_METHOD (ScanlineAddress_Row128_ThirdGroup)
    {
        // Scanline 128: group=2, subRow=0, lineInGroup=0
        // = 0x2000 + 0 + 2*40 + 0 = 0x2050
        Word addr = AppleHiResMode::ScanlineAddress (128, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2050), addr);
    }

    TEST_METHOD (ScanlineAddress_Row191_LastScanline)
    {
        // Scanline 191: group=2, subRow=7, lineInGroup=7
        // = 0x2000 + 7*1024 + 2*40 + 7*128
        // = 0x2000 + 7168 + 80 + 896 = 0x2000 + 8144 = 0x3FD0
        Word addr = AppleHiResMode::ScanlineAddress (191, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x3FD0), addr);
    }

    TEST_METHOD (ScanlineAddress_Page2Base)
    {
        Word addr = AppleHiResMode::ScanlineAddress (0, 0x4000);
        Assert::AreEqual (static_cast<Word> (0x4000), addr);

        Word addr64 = AppleHiResMode::ScanlineAddress (64, 0x4000);
        Assert::AreEqual (static_cast<Word> (0x4028), addr64);
    }

    TEST_METHOD (Render_AllZeros_AllBlack)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        // Zero out hi-res page 1
        for (Word addr = 0x2000; addr < 0x4000; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0xFFFFFFFF);

        hires.Render (nullptr, fb.data (), fbW, fbH);

        // All pixels should be black (0xFF000000)
        Assert::AreEqual (0xFF000000u, fb[0]);
        Assert::AreEqual (0xFF000000u, fb[fbW + 1]);
    }

    TEST_METHOD (Render_SinglePixelPalette0EvenCol_Violet)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x2000; addr < 0x4000; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        // Set bit 0 at scanline 0 byte 0: single pixel at column 0
        // Palette bit = 0 (bit 7 = 0), column 0 is even -> violet
        bus.WriteByte (0x2000, 0x01);

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        hires.Render (nullptr, fb.data (), fbW, fbH);

        // Pixel at screen col 0 -> fb[0,0] and fb[0,1] (2x scaled)
        // Violet = 0xFFFF44FD
        Assert::AreEqual (0xFFFF44FDu, fb[0],
            L"Single pixel palette 0 even col should be violet");
    }

    TEST_METHOD (Render_SinglePixelPalette1EvenCol_Blue)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x2000; addr < 0x4000; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        // Set bit 0 with palette bit (0x80): 0x81
        // Palette 1, column 0 is even -> blue
        bus.WriteByte (0x2000, 0x81);

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        hires.Render (nullptr, fb.data (), fbW, fbH);

        // Blue = 0xFF14CFFF
        Assert::AreEqual (0xFF14CFFFu, fb[0],
            L"Single pixel palette 1 even col should be blue");
    }

    TEST_METHOD (Render_AdjacentPixels_White)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x2000; addr < 0x4000; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        // Bits 0 and 1 set = adjacent pixels -> white
        bus.WriteByte (0x2000, 0x03);

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        hires.Render (nullptr, fb.data (), fbW, fbH);

        // Both pixels should be white (0xFFFFFFFF)
        // Pixel 0 at fb[0], pixel 1 at fb[2] (2x scaled)
        Assert::AreEqual (0xFFFFFFFFu, fb[0],
            L"First adjacent pixel should be white");
        Assert::AreEqual (0xFFFFFFFFu, fb[2],
            L"Second adjacent pixel should be white");
    }

    TEST_METHOD (GetActivePageAddress_ReturnsCorrectPages)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleHiResMode hires (bus);

        Assert::AreEqual (static_cast<Word> (0x2000), hires.GetActivePageAddress (false));
        Assert::AreEqual (static_cast<Word> (0x4000), hires.GetActivePageAddress (true));
    }
};
