#include "../CassoEmuCore/Pch.h"

#include <CppUnitTest.h>

#include "Core/MemoryBus.h"
#include "Devices/RamDevice.h"
#include "Video/AppleTextMode.h"
#include "Video/Apple80ColTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"
#include "Video/AppleDoubleHiResMode.h"
#include "Video/CharacterRomData.h"

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

        // Lo-res palette color 7 = Light Blue (kLoResColors[7] = 0xFF66AAFF)
        // Lo-res palette color 13 = Yellow (kLoResColors[13] = 0xFFFFFF00)
        uint32_t expectedTop    = 0xFF66AAFFu;  // color index 7
        uint32_t expectedBottom = 0xFFFFFF00u;  // color index 13

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
        // Violet = kNtscViolet (0xFFFF44FD in B8G8R8A8 byte layout)
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

        // Blue = kNtscBlue (0xFF14CFFF in B8G8R8A8 byte layout)
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

    TEST_METHOD (HiRes_NTSCArtifact_ProducesSixColorOutput)
    {
        // FR-018: hi-res NTSC artifact produces a 6-color palette.
        // Place patterns that yield each of black, white, violet, green,
        // blue, orange and assert all six colors appear in the framebuffer.
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x2000; addr < 0x4000; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        // All patterns on scanline 0 ($2000-$2027):
        //   byte 0 = $01: bit 0 -> x=0 isolated, palette 0, even col -> violet
        //   byte 1 = $01: bit 0 -> x=7 isolated, palette 0, odd  col -> green
        //   byte 2 = $03: bits 0+1 -> x=14,15 adjacent  -> white
        //   byte 3 = $81: bit 0 + palette -> x=21 isolated, palette 1, odd  -> orange
        //   byte 4 = $81: bit 0 + palette -> x=28 isolated, palette 1, even -> blue
        // Bytes 5+ = $00 -> all black (provides black coverage too).
        bus.WriteByte (0x2000, 0x01);
        bus.WriteByte (0x2001, 0x01);
        bus.WriteByte (0x2002, 0x03);
        bus.WriteByte (0x2003, 0x81);
        bus.WriteByte (0x2004, 0x81);

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        hires.Render (nullptr, fb.data (), fbW, fbH);

        bool sawBlack  = false;
        bool sawWhite  = false;
        bool sawViolet = false;
        bool sawGreen  = false;
        bool sawBlue   = false;
        bool sawOrange = false;

        for (uint32_t pixel : fb)
        {
            if (pixel == 0xFF000000u) { sawBlack  = true; }
            if (pixel == 0xFFFFFFFFu) { sawWhite  = true; }
            if (pixel == 0xFFFF44FDu) { sawViolet = true; }
            if (pixel == 0xFF14F53Cu) { sawGreen  = true; }
            if (pixel == 0xFF14CFFFu) { sawBlue   = true; }
            if (pixel == 0xFFFF6A3Cu) { sawOrange = true; }
        }

        Assert::IsTrue (sawBlack,  L"NTSC palette must include black");
        Assert::IsTrue (sawWhite,  L"NTSC palette must include white");
        Assert::IsTrue (sawViolet, L"NTSC palette must include violet");
        Assert::IsTrue (sawGreen,  L"NTSC palette must include green");
        Assert::IsTrue (sawBlue,   L"NTSC palette must include blue");
        Assert::IsTrue (sawOrange, L"NTSC palette must include orange");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  Apple80ColTextModeTests
//
////////////////////////////////////////////////////////////////////////////////

namespace Phase12VideoTestHelpers
{
    // Build a synthetic 4KB //e video ROM with two distinct character sets
    // so we can exercise ALTCHARSET switching deterministically.
    //
    //   Normal set (offset 0x0000-0x07FF): glyph row pattern 0x55 (alternating dots)
    //   Alt set    (offset 0x0800-0x0FFF): glyph row pattern 0x2A (inverse pattern)
    //
    // The Decode4K path XORs the source bytes with 0xFF, so we pre-invert.
    static std::vector<Byte> Build4KSyntheticCharRom (Byte normalDots, Byte altDots)
    {
        // //e enhanced video ROM layout per UTAIIe ch. 8 / AppleWin
        // userVideoRom4K (matched by CharacterRomData::Decode4K):
        //   Primary set [00..3F] inverse + [40..7F] flash share offsets
        //     0..0x1FF (the first half kB of the file).
        //   Primary set [80..FF] normal text from offsets 0x400..0x7FF.
        //   Alt set [00..FF] all read linearly from offsets 0..0x7FF
        //     (chars $00-$7F are MouseText / inverse, $80-$FF reuse the
        //     same primary normal-text glyphs).
        //   Second 2 KB of the file is unused by the standard //e ROM.
        //
        // This synthetic ROM makes the chars $00-$7F (which the alt set
        // reads from the FIRST half of the first 2 KB) decode to the
        // alt-dot pattern; the chars $80-$FF (shared between primary
        // and alt sets) decode to the normal-dot pattern. Tests that
        // compare primary vs alt rendering must use a char in $00-$7F
        // since $80-$FF is identical between the two sets.
        std::vector<Byte> raw (4096, 0xFF);
        Byte normalSrc = static_cast<Byte> (~normalDots & 0xFF);
        Byte altSrc    = static_cast<Byte> (~altDots & 0xFF);

        for (size_t i = 0; i < 0x400; i++)
        {
            raw[i] = altSrc;       // chars $00-$7F (in alt set; flash/inverse in primary)
        }

        for (size_t i = 0x400; i < 0x800; i++)
        {
            raw[i] = normalSrc;    // chars $80-$FF (shared by both sets)
        }

        return raw;
    }
}

TEST_CLASS (Apple80ColTextModeTests)
{
public:

    TEST_METHOD (TextMode80_RendersAuxMainInterleave)
    {
        // 80-col: aux at even screen columns, main at odd screen columns.
        // Screen col 0 reads from aux memory at $0400; col 1 reads from
        // main memory at $0400. Place a different glyph in each and
        // verify both produce pixels at their respective positions.
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        // Fill main memory text page 1 with normal spaces.
        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            bus.WriteByte (addr, 0xA0);
        }

        // Aux memory buffer: 1KB starting at offset $0400 covered.
        std::vector<Byte> auxBuf (0x10000, 0xA0);   // fill with normal spaces

        // Aux $0400 = normal 'A' ($C1) -> screen col 0
        // Main $0400 = normal 'B' ($C2) -> screen col 1
        auxBuf[0x0400] = 0xC1;
        bus.WriteByte (0x0400, 0xC2);

        Apple80ColTextMode text80 (bus);
        text80.SetAuxMemory (auxBuf.data ());

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0);

        text80.Render (nullptr, fb.data (), fbW, fbH);

        // Cell 0 occupies fb x=0..6 (7 dots wide in 80-col, no horizontal scaling).
        // Cell 1 occupies fb x=7..13.
        bool col0HasGreen = false;
        bool col1HasGreen = false;

        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 7; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u) { col0HasGreen = true; }
            }
            for (int x = 7; x < 14; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u) { col1HasGreen = true; }
            }
        }

        Assert::IsTrue (col0HasGreen, L"Aux 'A' should render at screen column 0");
        Assert::IsTrue (col1HasGreen, L"Main 'B' should render at screen column 1");
    }

    TEST_METHOD (TextMode80_AltCharsetSwitchesGlyphSet)
    {
        // Audit M13: ALTCHARSET=1 must select the alt char set glyphs.
        // Build a synthetic 4KB ROM where normal set lights every pixel
        // (0x7F) and alt set lights none (0x00). Toggling ALTCHARSET on a
        // normal char ($C1 'A') must change the pixel pattern.
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            bus.WriteByte (addr, 0xA0);
        }

        bus.WriteByte (0x0400, 0x40);   // main col 1 = char $40 ('@' inverse
                                        // in primary, MouseText 0 in alt)

        std::vector<Byte> raw = Phase12VideoTestHelpers::Build4KSyntheticCharRom (0x7F, 0x00);
        CharacterRomData rom;
        Assert::AreEqual (S_OK, rom.LoadFromMemory (raw.data (), raw.size ()));
        Assert::IsTrue (rom.HasAltCharSet (), L"4KB synthetic rom must report alt char set");

        Apple80ColTextMode text80 (bus, rom);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb1 (fbW * fbH, 0);
        std::vector<uint32_t> fb2 (fbW * fbH, 0);

        text80.SetAltCharSet (false);
        text80.Render (nullptr, fb1.data (), fbW, fbH);

        text80.SetAltCharSet (true);
        text80.Render (nullptr, fb2.data (), fbW, fbH);

        // Normal set lights every dot (greens), alt set lights none (blacks).
        // The two framebuffers must differ inside cell 1 (x=7..13).
        bool differs = false;

        for (int y = 0; y < 16 && !differs; y++)
        {
            for (int x = 7; x < 14 && !differs; x++)
            {
                if (fb1[y * fbW + x] != fb2[y * fbW + x])
                {
                    differs = true;
                }
            }
        }

        Assert::IsTrue (differs, L"ALTCHARSET toggle must change rendered glyph");
    }

    TEST_METHOD (TextMode80_FlashesWhenAltCharSetSelectsFlashingSet)
    {
        // Audit M14: when ALTCHARSET=0, chars $40-$7F flash. Render the
        // same char with two different m_flashOn states and assert the
        // framebuffer differs. (When ALTCHARSET=1, no flash — covered by
        // the alt-set test above which sets the same flash state implicitly.)
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            bus.WriteByte (addr, 0xA0);
        }

        // Main $0400 = $41 ('A' in flash range), screen col 1.
        bus.WriteByte (0x0400, 0x41);

        std::vector<Byte> raw = Phase12VideoTestHelpers::Build4KSyntheticCharRom (0x7F, 0x7F);
        CharacterRomData rom;
        Assert::AreEqual (S_OK, rom.LoadFromMemory (raw.data (), raw.size ()));

        Apple80ColTextMode text80 (bus, rom);

        text80.SetAltCharSet (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fbFlashOn  (fbW * fbH, 0);
        std::vector<uint32_t> fbFlashOff (fbW * fbH, 0);

        text80.SetFlashState (true);
        text80.RenderRowRange (0, 1, nullptr, fbFlashOn.data (), fbW, fbH);

        text80.SetFlashState (false);
        text80.RenderRowRange (0, 1, nullptr, fbFlashOff.data (), fbW, fbH);

        bool differs = false;

        for (int y = 0; y < 16 && !differs; y++)
        {
            for (int x = 7; x < 14 && !differs; x++)
            {
                if (fbFlashOn[y * fbW + x] != fbFlashOff[y * fbW + x])
                {
                    differs = true;
                }
            }
        }

        Assert::IsTrue (differs, L"FLASH char $41 must alternate when ALTCHARSET=0");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  Phase12 MixedModeCompositionTests
//
//  FR-017a / FR-020: the bottom 4 rows of MIXED-mode rendering must be
//  drawn through the same composed RenderRowRange helper used by the full
//  text-mode render path. These tests exercise the helper directly to
//  confirm:
//    * It writes only into the requested row range (composition contract)
//    * 80COL=0 path (AppleTextMode::RenderRowRange) draws 40-col text
//    * 80COL=1 path (Apple80ColTextMode::RenderRowRange) draws 80-col text
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (MixedModeCompositionTests)
{
public:

    TEST_METHOD (MixedMode_BottomFourRowsAre40ColWhen80ColClear)
    {
        // Build a //e-shaped scenario: graphics frame (filled red), then
        // route bottom 4 text rows through AppleTextMode::RenderRowRange
        // (40-col, the 80COL=0 dispatch). Only fbY in rows 20-23 should
        // change; rows 0-19 must remain untouched.
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            bus.WriteByte (addr, 0xA0);
        }

        // Place a normal 'A' at row 20 col 0.
        // Row 20 base = 0x0400 + 128*(20%8) + 40*(20/8)
        //             = 0x0400 + 128*4 + 40*2 = 0x0400 + 512 + 80 = 0x0650
        bus.WriteByte (0x0650, 0xC1);

        AppleTextMode text40 (bus);
        text40.SetPage2 (false);

        const int      fbW   = 560;
        const int      fbH   = 384;
        const uint32_t kRed  = 0xFFFF0000;
        std::vector<uint32_t> fb (fbW * fbH, kRed);

        text40.RenderRowRange (20, 24, nullptr, fb.data (), fbW, fbH);

        // Rows 0-19 (fbY 0..319) must be unchanged (still red).
        bool topUntouched = true;
        for (int y = 0; y < 320 && topUntouched; y++)
        {
            for (int x = 0; x < fbW && topUntouched; x++)
            {
                if (fb[y * fbW + x] != kRed) { topUntouched = false; }
            }
        }
        Assert::IsTrue (topUntouched, L"Rows 0-19 must stay red (untouched by row-range render)");

        // Rows 20-23 must contain green pixels from the 'A' glyph at col 0
        // (fbY 320-383, fbX 0..13).
        bool sawGreen = false;
        for (int y = 320; y < 384 && !sawGreen; y++)
        {
            for (int x = 0; x < 14 && !sawGreen; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u) { sawGreen = true; }
            }
        }
        Assert::IsTrue (sawGreen, L"40-col 'A' must render in row 20 of the framebuffer");
    }

    TEST_METHOD (MixedMode_BottomFourRowsAre80ColWhen80ColSet_RoutedThroughComposedRenderer)
    {
        // FR-017a: when 80COL is set, the bottom 4 rows route through
        // Apple80ColTextMode::RenderRowRange. Verify the helper writes
        // only into rows 20-23 and that aux/main interleaving applies
        // (proving we went through the composed 80-col code path).
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            bus.WriteByte (addr, 0xA0);
        }

        std::vector<Byte> auxBuf (0x10000, 0xA0);

        // Row 20 base = 0x0400 + 128*4 + 40*2 = 0x0650
        // 80-col screen col 0 = aux[$0650]; screen col 1 = main[$0650]
        auxBuf[0x0650]    = 0xC1;       // 'A' in aux  -> screen col 0
        bus.WriteByte (0x0650, 0xC2);   // 'B' in main -> screen col 1

        Apple80ColTextMode text80 (bus);
        text80.SetAuxMemory (auxBuf.data ());

        const int      fbW    = 560;
        const int      fbH    = 384;
        const uint32_t kBlue  = 0xFF0000FF;
        std::vector<uint32_t> fb (fbW * fbH, kBlue);

        text80.RenderRowRange (20, 24, nullptr, fb.data (), fbW, fbH);

        // Rows 0-19 must remain untouched (still blue).
        bool topUntouched = true;
        for (int y = 0; y < 320 && topUntouched; y++)
        {
            for (int x = 0; x < fbW && topUntouched; x++)
            {
                if (fb[y * fbW + x] != kBlue) { topUntouched = false; }
            }
        }
        Assert::IsTrue (topUntouched, L"Top 20 rows must be untouched by row-range 80-col render");

        // Both screen col 0 (aux 'A') and col 1 (main 'B') should produce
        // green pixels in row 20 — proving aux/main interleave.
        bool col0Green = false;
        bool col1Green = false;
        for (int y = 320; y < 336; y++)
        {
            for (int x = 0; x < 7; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u) { col0Green = true; }
            }
            for (int x = 7; x < 14; x++)
            {
                if (fb[y * fbW + x] == 0xFF00FF00u) { col1Green = true; }
            }
        }

        Assert::IsTrue (col0Green, L"Aux glyph at screen col 0 must render");
        Assert::IsTrue (col1Green, L"Main glyph at screen col 1 must render");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  AppleDoubleHiResModeTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleDoubleHiResModeTests)
{
public:

    TEST_METHOD (DHR_AuxMainInterleaveProduces16ColorOutput)
    {
        // FR-019, audit M8: DHR is 560x192 with 4-bit-nibble 16-color cells
        // sourced from interleaved aux+main 7-bit bytes. Place patterns
        // that exercise multiple distinct 4-bit nibble values across the
        // first scanline and assert at least 4 distinct DHR palette colors
        // appear (proves true 16-color rendering, not monochrome stub).
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x2000; addr < 0x4000; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        std::vector<Byte> auxBuf (0x10000, 0x00);

        // Scanline 0 layout (aux+main interleaved, 7 dots each):
        //   aux[$2000]=0x55 (0b1010101)  main[$2000]=0x2A (0b0101010)
        //   aux[$2001]=0x7F (all on)     main[$2001]=0x00 (all off)
        //   aux[$2002]=0x33              main[$2002]=0x66
        //
        // Dots per scanline: 14*40 = 560; 4-bit nibbles → 140 cells.
        auxBuf[0x2000] = 0x55;
        bus.WriteByte (0x2000, 0x2A);

        auxBuf[0x2001] = 0x7F;
        bus.WriteByte (0x2001, 0x00);

        auxBuf[0x2002] = 0x33;
        bus.WriteByte (0x2002, 0x66);

        AppleDoubleHiResMode dhr (bus);
        dhr.SetAuxMemory (auxBuf.data ());
        dhr.SetPage2 (false);

        const int fbW = 560;
        const int fbH = 384;
        std::vector<uint32_t> fb (fbW * fbH, 0xFFCCCCCC);

        dhr.Render (nullptr, fb.data (), fbW, fbH);

        // Sample first 56 pixels of scanline 0 (covers ~14 nibbles) and
        // collect distinct colors. Must include at least 4 distinct
        // palette entries to prove 16-color decoding.
        std::unordered_map<uint32_t, int> distinctColors;

        for (int x = 0; x < 56; x++)
        {
            distinctColors[fb[0 * fbW + x]] = 1;
        }

        Assert::IsTrue (distinctColors.size () >= 4,
            L"DHR must produce at least 4 distinct colors from a varied test pattern");

        // The all-off region (aux=0x7F, main=0x00 packs 7 ones+7 zeros)
        // must include white (nibble 0xF) somewhere within bytes-1..2.
        bool sawWhite = false;
        for (int x = 14; x < 28 && !sawWhite; x++)
        {
            if (fb[0 * fbW + x] == 0xFFFFFFFFu) { sawWhite = true; }
        }
        Assert::IsTrue (sawWhite, L"DHR all-on aux byte must include white cells");

        // The all-zero scanline 1 must be all black (proves DHR didn't
        // bleed pixels from scanline 0).
        bool sl1AllBlack = true;
        for (int x = 0; x < 560 && sl1AllBlack; x++)
        {
            if (fb[2 * fbW + x] != 0xFF000000u) { sl1AllBlack = false; }
        }
        Assert::IsTrue (sl1AllBlack, L"All-zero scanline 1 must be all black");
    }

    TEST_METHOD (DHR_GetActivePageAddress_ReturnsCorrectPages)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleDoubleHiResMode dhr (bus);

        Assert::AreEqual (static_cast<Word> (0x2000), dhr.GetActivePageAddress (false));
        Assert::AreEqual (static_cast<Word> (0x4000), dhr.GetActivePageAddress (true));
    }
};
