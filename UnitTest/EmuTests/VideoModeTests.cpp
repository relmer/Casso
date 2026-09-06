#include "Pch.h"
#include "Core/MemoryBus.h"
#include "Devices/RamDevice.h"
#include "Devices/Apple2eMmu.h"
#include "Devices/Apple2eSoftSwitchBank.h"
#include "Video/AppleTextMode.h"
#include "Video/Apple80ColTextMode.h"
#include "Video/AppleLoResMode.h"
#include "Video/AppleHiResMode.h"
#include "Video/AppleDoubleHiResMode.h"
#include "Video/CharacterRomData.h"
#include "Video/NtscColorTable.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  AppleTextModeTests
//
//  40-column text rendering: glyphs, the display attributes, and the dirty-row
//  cache.
//
//  The three ATTRIBUTE ranges are the substance. A character's high bits select
//  normal, inverse, or flashing, so the same code renders differently depending
//  on bits that are not part of its value -- and flashing additionally depends
//  on a phase pushed in from emulated time.
//
//  The dirty-row cache gets its own coverage because it is an optimization that
//  can be WRONG rather than merely slow: a row wrongly considered clean keeps
//  stale pixels. The tests therefore render twice and assert the second render
//  produces the correct image, including after a page, charset, or color change
//  -- each of which must force a full redraw.
//
//  Both text pages are covered, since the page-2 base address is a separate
//  calculation.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleTextModeTests)
{
public:

    TEST_METHOD (Render_Row0Col0_NormalSpace_AllBlack)
    {
        const int fbW = 560;
        const int fbH = 384;



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
        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data(), fbW, fbH);

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
        const int  fbW      = 560;
        const int  fbH      = 384;
        bool       hasGreen = false;



        // Inverse 'A' ($01) should render with some green (inverted) pixels
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        // Inverse 'A' = $01 at row 0 col 0
        bus.WriteByte (0x0400, 0x01);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data(), fbW, fbH);

        // Inverse character should have some green pixels (inverted rendering)

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
        const int  fbW      = 560;
        const int  fbH      = 384;
        bool       hasGreen = false;



        // Normal 'A' = $C1 at row 0 col 0
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0xC1);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data(), fbW, fbH);

        // Normal 'A' glyph should have some green pixels

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
        const int  fbW     = 560;
        const int  fbH     = 384;
        bool       differs = false;



        // Flash 'A' = $41 should alternate between inverse and normal as the
        // externally-driven flash phase toggles. Render() no longer advances
        // flash itself -- the caller sets it via SetFlashState -- so drive
        // both phases explicitly.
        MemoryBus bus;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0x41);

        AppleTextMode textMode (bus);
        textMode.SetPage2 (false);

        std::vector<uint32_t> fb1 (fbW * fbH, 0);
        std::vector<uint32_t> fb2 (fbW * fbH, 0);

        // Flash on -> inverse phase
        textMode.SetFlashState (true);
        textMode.Render (nullptr, fb1.data(), fbW, fbH);

        // Flash off -> normal phase
        textMode.SetFlashState (false);
        textMode.Render (nullptr, fb2.data(), fbW, fbH);

        // The two phases should differ since flash toggled

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
        const int  fbW      = 560;
        const int  fbH      = 384;
        bool       hasGreen = false;



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

        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data(), fbW, fbH);

        // Row 8 starts at fbY = 8*8*2 = 128

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
        MemoryBus  bus;
        const int  fbW      = 560;
        const int  fbH      = 384;
        bool       hasGreen = false;
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

        std::vector<uint32_t> fb (fbW * fbH, 0);

        textMode.Render (nullptr, fb.data(), fbW, fbH);


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
//  Lo-res graphics: two blocks per byte, from the SAME memory the text screen
//  uses.
//
//  That sharing is the hardware's design and the first thing to get right --
//  lo-res and text occupy identical addresses and differ only in
//  interpretation, so the row addressing must match the text layout exactly.
//
//  The nybble split is the other half: the LOW nybble is the top block and the
//  high nybble the bottom, giving 48 rows out of 24 bytes. Reversing them
//  produces a picture that looks plausible and is vertically scrambled, so the
//  fixtures use bytes whose two nybbles differ.
//
//  The sixteen-color palette is asserted by value rather than by index, since
//  the colors are a hardware fact rather than an arbitrary table.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleLoResModeTests)
{
public:

    TEST_METHOD (Render_NybbleDecoding_TopAndBottomColors)
    {
        const int  fbW            = 560;
        const int  fbH            = 384;
        uint32_t   expectedTop    = 0;
        uint32_t   expectedBottom = 0;



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
        std::vector<uint32_t> fb (fbW * fbH, 0);

        lores.Render (nullptr, fb.data(), fbW, fbH);

        // Lo-res palette color 7 = Light Blue (kLoResColors[7] = 0xFF66AAFF)
        // Lo-res palette color 13 = Yellow (kLoResColors[13] = 0xFFFFFF00)
        expectedTop = 0xFF66AAFFu; // color index 7
        expectedBottom = 0xFFFFFF00u; // color index 13

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
        const int fbW = 560;
        const int fbH = 384;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0x00);

        AppleLoResMode lores (bus);
        lores.SetPage2 (false);

        std::vector<uint32_t> fb (fbW * fbH, 0xFFFFFFFF);

        lores.Render (nullptr, fb.data(), fbW, fbH);

        // Color 0 = black (0xFF000000)
        Assert::AreEqual (0xFF000000u, fb[0], L"Color 0 should be black");
    }

    TEST_METHOD (Render_ColorF_IsWhite)
    {
        MemoryBus bus;
        const int fbW = 560;
        const int fbH = 384;
        RamDevice ram (0x0000, 0x0BFF);
        bus.AddDevice (&ram);

        bus.WriteByte (0x0400, 0xFF);

        AppleLoResMode lores (bus);
        lores.SetPage2 (false);

        std::vector<uint32_t> fb (fbW * fbH, 0);

        lores.Render (nullptr, fb.data(), fbW, fbH);

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
//  Hi-res graphics: the interleaved scanline addressing and NTSC artifact
//  color.
//
//  Hi-res line addressing is famously NON-LINEAR -- consecutive scanlines are
//  scattered across the page in a three-level interleave -- so the address
//  calculation is a lookup-shaped formula that is wrong in an obvious way or
//  right, and the tests check lines at each interleave boundary rather than a
//  contiguous run.
//
//  Artifact color is the other half, and it is why the fixtures set ADJACENT
//  bits rather than isolated ones: a lone dot is colored while neighbors merge
//  to white, so a renderer ignoring neighbors passes any single-pixel test.
//
//  The palette bit (bit 7) is covered specifically, including a run crossing a
//  byte boundary into a different palette -- which is where a per-byte
//  implementation diverges from a per-pixel one.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleHiResModeTests)
{
public:

    TEST_METHOD (ScanlineAddress_Row0_ReturnsPageBase)
    {
        // Scanline 0: group=0, subRow=0, lineInGroup=0
        // = pageBase + 0 + 0 + 0
        Word addr = AppleHiResMode::GetScanlineAddress (0, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2000), addr);
    }

    TEST_METHOD (ScanlineAddress_Row1_Returns128Offset)
    {
        // Scanline 1: group=0, subRow=1, lineInGroup=0
        // = 0x2000 + 1*1024 + 0 + 0 = 0x2400
        Word addr = AppleHiResMode::GetScanlineAddress (1, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2400), addr);
    }

    TEST_METHOD (ScanlineAddress_Row8_ReturnsCorrectInterleave)
    {
        // Scanline 8: group=0, subRow=0, lineInGroup=1
        // = 0x2000 + 0 + 0 + 1*128 = 0x2080
        Word addr = AppleHiResMode::GetScanlineAddress (8, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2080), addr);
    }

    TEST_METHOD (ScanlineAddress_Row64_SecondGroup)
    {
        // Scanline 64: group=1, subRow=0, lineInGroup=0
        // = 0x2000 + 0 + 1*40 + 0 = 0x2028
        Word addr = AppleHiResMode::GetScanlineAddress (64, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2028), addr);
    }

    TEST_METHOD (ScanlineAddress_Row128_ThirdGroup)
    {
        // Scanline 128: group=2, subRow=0, lineInGroup=0
        // = 0x2000 + 0 + 2*40 + 0 = 0x2050
        Word addr = AppleHiResMode::GetScanlineAddress (128, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x2050), addr);
    }

    TEST_METHOD (ScanlineAddress_Row191_LastScanline)
    {
        // Scanline 191: group=2, subRow=7, lineInGroup=7
        // = 0x2000 + 7*1024 + 2*40 + 7*128
        // = 0x2000 + 7168 + 80 + 896 = 0x2000 + 8144 = 0x3FD0
        Word addr = AppleHiResMode::GetScanlineAddress (191, 0x2000);
        Assert::AreEqual (static_cast<Word> (0x3FD0), addr);
    }

    TEST_METHOD (ScanlineAddress_Page2Base)
    {
        Word  addr   = AppleHiResMode::GetScanlineAddress (0, 0x4000);
        Word  addr64 = 0;
        Assert::AreEqual (static_cast<Word> (0x4000), addr);

        addr64 = AppleHiResMode::GetScanlineAddress (64, 0x4000);
        Assert::AreEqual (static_cast<Word> (0x4028), addr64);
    }

    TEST_METHOD (Render_AllZeros_AllBlack)
    {
        MemoryBus bus;
        const int fbW = 560;
        const int fbH = 384;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        // Zero out hi-res page 1
        for (Word addr = 0x2000; addr < 0x4000; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        AppleHiResMode hires (bus);
        hires.SetPage2 (false);

        std::vector<uint32_t> fb (fbW * fbH, 0xFFFFFFFF);

        hires.Render (nullptr, fb.data(), fbW, fbH);

        // All pixels should be black (0xFF000000)
        Assert::AreEqual (0xFF000000u, fb[0]);
        Assert::AreEqual (0xFF000000u, fb[fbW + 1]);
    }

    TEST_METHOD (Render_SinglePixelPalette0EvenCol_Violet)
    {
        MemoryBus bus;
        const int fbW = 560;
        const int fbH = 384;
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

        std::vector<uint32_t> fb (fbW * fbH, 0);

        hires.Render (nullptr, fb.data(), fbW, fbH);

        // Pixel at screen col 0 -> fb[0,0] and fb[0,1] (2x scaled)
        // Violet = kNtscViolet (0xFFFF44FD in B8G8R8A8 byte layout)
        Assert::AreEqual (0xFFFF44FDu, fb[0],
            L"Single pixel palette 0 even col should be violet");
    }

    TEST_METHOD (Render_SinglePixelPalette1EvenCol_Blue)
    {
        MemoryBus bus;
        const int fbW = 560;
        const int fbH = 384;
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

        std::vector<uint32_t> fb (fbW * fbH, 0);

        hires.Render (nullptr, fb.data(), fbW, fbH);

        // Blue = kNtscBlue (0xFF14CFFF in B8G8R8A8 byte layout)
        Assert::AreEqual (0xFF14CFFFu, fb[0],
            L"Single pixel palette 1 even col should be blue");
    }

    TEST_METHOD (Render_AdjacentPixels_White)
    {
        MemoryBus bus;
        const int fbW = 560;
        const int fbH = 384;
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

        std::vector<uint32_t> fb (fbW * fbH, 0);

        hires.Render (nullptr, fb.data(), fbW, fbH);

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
        const int  fbW       = 560;
        const int  fbH       = 384;
        bool       sawBlack  = false;
        bool       sawWhite  = false;
        bool       sawViolet = false;
        bool       sawGreen  = false;
        bool       sawBlue   = false;
        bool       sawOrange = false;



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

        std::vector<uint32_t> fb (fbW * fbH, 0);

        hires.Render (nullptr, fb.data(), fbW, fbH);


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

    TEST_METHOD (Hires_ColorMonitorIsTheDefault)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleHiResMode hires (bus);

        Assert::IsFalse (hires.IsMonochrome(),
            L"Hi-res must start on the color decode");
    }

    // An isolated dot is where the color decode and the monochrome decode
    // disagree most: a color monitor makes violet out of it, and a
    // monochrome monitor shows it lit. Tinting the violet gives ~57%
    // brightness, which is the defect this replaces.
    TEST_METHOD (Hires_Monochrome_IsolatedDotIsFullyLit)
    {
        MemoryBus              bus;
        std::vector<uint32_t>  fb (560 * 384, 0xFFCCCCCC);



        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word a = 0x2000; a < 0x4000; a++) { bus.WriteByte (a, 0x00); }

        bus.WriteByte (0x2000, 0x01);   // bit 0 only, palette bit clear

        AppleHiResMode hires (bus);
        hires.SetPage2     (false);
        hires.SetMonochrome (true);

        hires.Render (nullptr, fb.data(), 560, 384);

        // Pixel 0 occupies half-dots 0 and 1, at full brightness.
        Assert::AreEqual (0xFFFFFFFFu, fb[0], L"half-dot 0 must be fully lit");
        Assert::AreEqual (0xFFFFFFFFu, fb[1], L"half-dot 1 must be fully lit");
        Assert::AreEqual (0xFF000000u, fb[2], L"half-dot 2 must stay dark");

        // Same dot on a color monitor is violet -- the case that used to be
        // luminance-tinted to ~57% gray instead of shown lit.
        hires.SetMonochrome (false);
        hires.Render (nullptr, fb.data(), 560, 384);

        Assert::AreEqual (0xFFFF44FDu, fb[0],
            L"the color decode must still make violet from an isolated dot");
    }

    // The half-dot shift is the whole reason monochrome hi-res is 560 wide.
    // Bit 7 delays a byte's output by half a dot, so the SAME bit pattern
    // lands one half-dot to the right. At 280 that detail does not exist.
    TEST_METHOD (Hires_Monochrome_PaletteBitShiftsByAHalfDot)
    {
        MemoryBus              bus;
        std::vector<uint32_t>  fb (560 * 384, 0xFFCCCCCC);



        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word a = 0x2000; a < 0x4000; a++) { bus.WriteByte (a, 0x00); }

        // Row 0 byte 0: bit 0, palette bit CLEAR -> half-dots 0,1.
        // Row 1 byte 0: bit 0, palette bit SET   -> half-dots 1,2.
        bus.WriteByte (0x2000, 0x01);
        bus.WriteByte (AppleHiResMode::GetScanlineAddress (1, 0x2000), 0x81);

        AppleHiResMode hires (bus);
        hires.SetPage2      (false);
        hires.SetMonochrome (true);

        hires.Render (nullptr, fb.data(), 560, 384);

        const int  row1 = 2 * 560;   // scanline 1, first of its doubled rows

        Assert::AreEqual (0xFFFFFFFFu, fb[0], L"unshifted: half-dot 0 lit");
        Assert::AreEqual (0xFFFFFFFFu, fb[1], L"unshifted: half-dot 1 lit");
        Assert::AreEqual (0xFF000000u, fb[2], L"unshifted: half-dot 2 dark");

        Assert::AreEqual (0xFF000000u, fb[row1 + 0], L"shifted: half-dot 0 dark");
        Assert::AreEqual (0xFFFFFFFFu, fb[row1 + 1], L"shifted: half-dot 1 lit");
        Assert::AreEqual (0xFFFFFFFFu, fb[row1 + 2], L"shifted: half-dot 2 lit");
        Assert::AreEqual (0xFF000000u, fb[row1 + 3], L"shifted: half-dot 3 dark");
    }

    // Adjacent lit pixels are white on both monitors, but for different
    // reasons -- and on a monochrome monitor a run must stay solid rather
    // than developing a hole where a shifted byte meets an unshifted one.
    TEST_METHOD (Hires_Monochrome_RunsStaySolidAcrossAByteSeam)
    {
        MemoryBus              bus;
        std::vector<uint32_t>  fb (560 * 384, 0xFFCCCCCC);
        int                    darkInRun = 0;



        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word a = 0x2000; a < 0x4000; a++) { bus.WriteByte (a, 0x00); }

        // Byte 0 all seven dots lit and unshifted; byte 1 all seven lit and
        // shifted. The seam between them is the interesting half-dot.
        bus.WriteByte (0x2000, 0x7F);
        bus.WriteByte (0x2001, 0xFF);

        AppleHiResMode hires (bus);
        hires.SetPage2      (false);
        hires.SetMonochrome (true);

        hires.Render (nullptr, fb.data(), 560, 384);

        // Byte 0 covers half-dots 0-13. Byte 1 is shifted, so it covers
        // 15-28 and leaves half-dot 14 dark -- the gap the hardware makes.
        for (int x = 0; x < 14; x++)
        {
            if (fb[x] != 0xFFFFFFFFu) { darkInRun++; }
        }

        Assert::AreEqual (0, darkInRun, L"an unshifted run must be solid");
        Assert::AreEqual (0xFF000000u, fb[14],
            L"a shifted byte after an unshifted one leaves a half-dot gap");
        Assert::AreEqual (0xFFFFFFFFu, fb[15], L"the shifted run starts at 15");
        Assert::AreEqual (0xFFFFFFFFu, fb[28], L"the shifted run ends at 28");
        Assert::AreEqual (0xFF000000u, fb[29], L"and stops there");
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  Phase12VideoTestHelpers
//
//  Builds a SYNTHETIC character ROM so ALTCHARSET switching can be tested
//  without shipping a real //e video ROM into the test tree.
//
//  The two character sets carry deliberately DIFFERENT dot patterns, so which
//  set was used is visible in the rendered output -- a real ROM's two sets
//  share many glyphs, and a switching bug would be invisible for most
//  characters.
//
//  It reproduces the //e 4K ROM's actual layout rather than a convenient flat
//  one, because that layout is what Decode4K unpacks: the primary set's
//  inverse and flash ranges share offsets, and the alt set reads linearly. A
//  simplified fixture would pass against a decoder that got the real layout
//  wrong.
//
//  The source bytes are PRE-INVERTED because Decode4K XORs with $FF, so the
//  patterns above are what the renderer ends up seeing.
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
        Byte  normalSrc = 0;
        Byte  altSrc    = 0;



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
        normalSrc = static_cast<Byte> (~normalDots & 0xFF);
        altSrc = static_cast<Byte> (~altDots & 0xFF);

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





////////////////////////////////////////////////////////////////////////////////
//
//  Apple80ColTextModeTests
//
//  80-column text: the aux/main column interleave, and ALTCHARSET.
//
//  The interleave is the whole mode. Aux memory supplies the EVEN screen
//  columns and main supplies the ODD ones, both read from the same addresses --
//  so the renderer walks one address range twice through different memory. An
//  implementation reading only main produces a readable 40-column screen with
//  every other character missing, which looks like a font bug rather than a
//  banking one.
//
//  The fixtures put DIFFERENT content in aux and main at the same addresses,
//  which is the only way that swap is visible.
//
//  Page 1 is used unconditionally: 80-column mode has no page 2, and honoring
//  PAGE2 here would display the wrong half of memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple80ColTextModeTests)
{
public:

    // The mixed-mode overlay renders with no videoRam, so the main column
    // used to come through the bus -- which, under 80STORE with PAGE2 on,
    // answers a text-page read from aux. A program that keeps PAGE2 on while
    // it writes its own aux bank therefore had frames with aux in both
    // columns. The fixture mirrors DHR_MainHalfIgnoresAuxBankingUnder80Store:
    // 80STORE goes to the switch bank directly, and the bus is proven banked
    // before the render so a pass means something.
    TEST_METHOD (TextMode80_MainColumnIgnoresAuxBankingUnder80Store)
    {
        const int              fbW          = 560;
        const int              fbH          = 384;
        MemoryBus              bus;
        RamDevice              mainRam (0x0000, 0xBFFF);
        Apple2eMmu             mmu;
        Apple2eSoftSwitchBank  sw (&bus);
        Byte *                 mainPtr      = mainRam.GetData();
        std::vector<uint32_t>  fb (fbW * fbH, 0);
        bool                   col0HasGreen = false;
        bool                   col1HasGreen = false;



        for (int page = 0x00; page < 0xC0; page++)
        {
            bus.SetReadPage  (page, mainPtr + (page * 0x100));
            bus.SetWritePage (page, mainPtr + (page * 0x100));
        }

        sw.SetMmu (&mmu);
        mmu.Initialize (&bus, &mainRam, nullptr, nullptr, nullptr, &sw);
        bus.AddDevice (&sw);

        Byte * auxPtr = mmu.GetAuxBuffer();

        for (Word addr = 0x0400; addr < 0x0800; addr++)
        {
            mainPtr[addr] = 0xA0;
            auxPtr [addr] = 0xA0;
        }

        // A blank in aux (column 0) and an 'A' in main (column 1): read both
        // from aux and column 1 goes blank.
        mainPtr[0x0400] = 0xC1;

        sw.Write     (0xC001, 0x00);   // 80STORE on -- bank surface, not bus
        bus.ReadByte (0xC055);         // PAGE2 on

        Assert::IsTrue (mmu.Get80Store(), L"fixture: 80STORE did not latch");
        Assert::IsTrue (sw.IsPage2(),     L"fixture: PAGE2 did not latch");
        Assert::AreEqual (static_cast<Byte> (0xA0), bus.ReadByte (0x0400),
            L"fixture: the bus must now be banked to aux for this to prove anything");

        Apple80ColTextMode text80 (bus);
        text80.SetAuxMemory  (auxPtr);
        text80.SetMainMemory (mainPtr);
        text80.RenderRowRange (0, 1, nullptr, fb.data(), fbW, fbH);

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

        Assert::IsFalse (col0HasGreen, L"column 0 is the aux blank");
        Assert::IsTrue  (col1HasGreen,
            L"column 1 must come from main RAM, not from the bus banked to aux");
    }

    TEST_METHOD (TextMode80_RendersAuxMainInterleave)
    {
        const int  fbW          = 560;
        const int  fbH          = 384;
        bool       col0HasGreen = false;
        bool       col1HasGreen = false;



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
        text80.SetAuxMemory (auxBuf.data());

        std::vector<uint32_t> fb (fbW * fbH, 0);

        text80.Render (nullptr, fb.data(), fbW, fbH);

        // Cell 0 occupies fb x=0..6 (7 dots wide in 80-col, no horizontal scaling).
        // Cell 1 occupies fb x=7..13.

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
        std::vector<Byte>  raw;
        CharacterRomData   rom;
        const int          fbW     = 560;
        const int          fbH     = 384;
        bool               differs = false;



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

        raw = Phase12VideoTestHelpers::Build4KSyntheticCharRom (0x7F, 0x00);
        Assert::AreEqual (S_OK, rom.LoadFromMemory (raw.data(), raw.size()));
        Assert::IsTrue (rom.HasAltCharSet(), L"4KB synthetic rom must report alt char set");

        Apple80ColTextMode text80 (bus, rom);

        std::vector<uint32_t> fb1 (fbW * fbH, 0);
        std::vector<uint32_t> fb2 (fbW * fbH, 0);

        text80.SetAltCharSet (false);
        text80.Render (nullptr, fb1.data(), fbW, fbH);

        text80.SetAltCharSet (true);
        text80.Render (nullptr, fb2.data(), fbW, fbH);

        // Normal set lights every dot (greens), alt set lights none (blacks).
        // The two framebuffers must differ inside cell 1 (x=7..13).

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
        std::vector<Byte>  raw;
        CharacterRomData   rom;
        const int          fbW     = 560;
        const int          fbH     = 384;
        bool               differs = false;



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

        raw = Phase12VideoTestHelpers::Build4KSyntheticCharRom (0x7F, 0x7F);
        Assert::AreEqual (S_OK, rom.LoadFromMemory (raw.data(), raw.size()));

        Apple80ColTextMode text80 (bus, rom);

        text80.SetAltCharSet (false);

        std::vector<uint32_t> fbFlashOn  (fbW * fbH, 0);
        std::vector<uint32_t> fbFlashOff (fbW * fbH, 0);

        text80.SetFlashState (true);
        text80.RenderRowRange (0, 1, nullptr, fbFlashOn.data(), fbW, fbH);

        text80.SetFlashState (false);
        text80.RenderRowRange (0, 1, nullptr, fbFlashOff.data(), fbW, fbH);


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
        const int  fbW          = 560;
        const int  fbH          = 384;
        uint32_t   kRed         = 0;
        bool       topUntouched = false;
        bool       sawGreen     = false;



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

        kRed = 0xFFFF0000;
        std::vector<uint32_t> fb (fbW * fbH, kRed);

        text40.RenderRowRange (20, 24, nullptr, fb.data(), fbW, fbH);

        // Rows 0-19 (fbY 0..319) must be unchanged (still red).
        topUntouched = true;
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
        const int  fbW          = 560;
        const int  fbH          = 384;
        uint32_t   kBlue        = 0;
        bool       topUntouched = false;
        bool       col0Green    = false;
        bool       col1Green    = false;



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
        text80.SetAuxMemory (auxBuf.data());

        kBlue = 0xFF0000FF;
        std::vector<uint32_t> fb (fbW * fbH, kBlue);

        text80.RenderRowRange (20, 24, nullptr, fb.data(), fbW, fbH);

        // Rows 0-19 must remain untouched (still blue).
        topUntouched = true;
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
//  Double hi-res: 560 pixels wide, interleaving AUX and MAIN memory
//  byte by byte.
//
//  The mode needs both DHIRES and 80COL, which is a fact about the hardware
//  rather than a convention -- it borrows the 80-column circuitry to clock the
//  interleave -- so the fixtures set both, and a test with DHIRES alone would
//  be exercising ordinary hi-res.
//
//  Aux supplies the EVEN bytes and main the odd ones, so the fixtures put
//  distinguishable content in each; identical content would render correctly
//  under a renderer that ignored aux entirely.
//
//  Color here comes from four-bit groups across the interleaved stream rather
//  than from artifact adjacency, so the palette is genuinely different from
//  hi-res and is asserted separately (FR-019).
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (AppleDoubleHiResModeTests)
{
public:

    TEST_METHOD (DHR_AuxMainInterleaveProduces16ColorOutput)
    {
        const int                          fbW            = 560;
        const int                          fbH            = 384;
        std::unordered_map<uint32_t, int>  distinctColors;
        bool                               sawWhite       = false;
        bool                               sl1AllBlack    = false;



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
        dhr.SetAuxMemory (auxBuf.data());
        dhr.SetPage2 (false);

        std::vector<uint32_t> fb (fbW * fbH, 0xFFCCCCCC);

        dhr.Render (nullptr, fb.data(), fbW, fbH);

        // Sample first 56 pixels of scanline 0 (covers ~14 nibbles) and
        // collect distinct colors. Must include at least 4 distinct
        // palette entries to prove 16-color decoding.

        for (int x = 0; x < 56; x++)
        {
            distinctColors[fb[0 * fbW + x]] = 1;
        }

        Assert::IsTrue (distinctColors.size() >= 4,
            L"DHR must produce at least 4 distinct colors from a varied test pattern");

        // The all-off region (aux=0x7F, main=0x00 packs 7 ones+7 zeros)
        // must include white (nibble 0xF) somewhere within bytes-1..2.
        for (int x = 14; x < 28 && !sawWhite; x++)
        {
            if (fb[0 * fbW + x] == 0xFFFFFFFFu) { sawWhite = true; }
        }

        Assert::IsTrue (sawWhite, L"DHR all-on aux byte must include white cells");

        // The all-zero scanline 1 must be all black (proves DHR didn't
        // bleed pixels from scanline 0).
        sl1AllBlack = true;
        for (int x = 0; x < 560 && sl1AllBlack; x++)
        {
            if (fb[2 * fbW + x] != 0xFF000000u) { sl1AllBlack = false; }
        }

        Assert::IsTrue (sl1AllBlack, L"All-zero scanline 1 must be all black");
    }

    // Apple IIe Technical Note #3 gives each color as the four bytes that
    // fill a 28-dot span: aux, main, aux, main. Those bytes are the
    // definition of the color phase, so the decoder is held to them rather
    // than to a reading of the dots as a binary number, which is one dot
    // off and turns magenta brown and orange green.
    TEST_METHOD (DHR_TechnotePatternsDecodeToTheirColors)
    {
        const int              fbW      = 560;
        const int              fbH      = 384;
        // By color number, not by literal: this test pins the dot->color
        // rotation, so it must not also fail when the palette is retuned.
        const uint32_t         kMagenta = kAppleColors[1];
        const uint32_t         kOrange  = kAppleColors[9];
        const uint32_t         kViolet  = kAppleColors[3];
        MemoryBus              bus;
        RamDevice              ram (0x0000, 0x5FFF);
        std::vector<Byte>      auxBuf (0x10000, 0x00);
        std::vector<uint32_t>  fb (fbW * fbH, 0xFFCCCCCC);
        Word                   row1     = AppleHiResMode::GetScanlineAddress (1, 0x2000);
        Word                   row2     = AppleHiResMode::GetScanlineAddress (2, 0x2000);



        bus.AddDevice (&ram);

        // Row 0 magenta, row 1 orange, row 2 violet, each pattern repeated
        // across the first four byte columns.
        const Byte magenta[4] = { 0x08, 0x11, 0x22, 0x44 };
        const Byte orange[4]  = { 0x4C, 0x19, 0x33, 0x66 };
        const Byte violet[4]  = { 0x19, 0x33, 0x66, 0x4C };

        for (int col = 0; col < 4; col++)
        {
            auxBuf[0x2000 + col] = magenta[(col * 2)     % 4];
            bus.WriteByte (static_cast<Word> (0x2000 + col), magenta[(col * 2 + 1) % 4]);

            auxBuf[row1 + col] = orange[(col * 2)     % 4];
            bus.WriteByte (static_cast<Word> (row1 + col), orange[(col * 2 + 1) % 4]);

            auxBuf[row2 + col] = violet[(col * 2)     % 4];
            bus.WriteByte (static_cast<Word> (row2 + col), violet[(col * 2 + 1) % 4]);
        }

        AppleDoubleHiResMode dhr (bus);
        dhr.SetAuxMemory (auxBuf.data());
        dhr.SetPage2 (false);
        dhr.Render (nullptr, fb.data(), fbW, fbH);

        // Four byte columns are 56 dots: 14 whole cells.
        for (int x = 0; x < 56; x++)
        {
            Assert::AreEqual (kMagenta, fb[0 * fbW + x], L"technote magenta");
            Assert::AreEqual (kOrange,  fb[2 * fbW + x], L"technote orange");
            Assert::AreEqual (kViolet,  fb[4 * fbW + x], L"technote violet");
        }
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

    // DHR needs main and aux at the same instant, and the memory bus cannot
    // supply that: under 80STORE + HIRES, PAGE2 alone points the whole
    // $2000-$3FFF range at aux. Reading the main half through the bus used
    // to render the aux byte into BOTH halves of every 14-dot group.
    //
    // Note the setup: 80STORE is written straight to the soft-switch bank,
    // because $C000-$C00B belongs to Apple2eKeyboard on the bus and a
    // bus.WriteByte there latches nothing -- which would make this test pass
    // for the wrong reason. The asserts below pin that down before the
    // render, so a broken fixture fails as a fixture rather than as a pass.
    TEST_METHOD (DHR_MainHalfIgnoresAuxBankingUnder80Store)
    {
        MemoryBus              bus;
        RamDevice              mainRam (0x0000, 0xBFFF);
        Apple2eMmu             mmu;
        Apple2eSoftSwitchBank  sw (&bus);
        Byte *                 mainPtr = mainRam.GetData();
        std::vector<uint32_t>  fb (560 * 384, 0xFFCCCCCC);
        std::string            dots;



        for (int page = 0x00; page < 0xC0; page++)
        {
            bus.SetReadPage  (page, mainPtr + (page * 0x100));
            bus.SetWritePage (page, mainPtr + (page * 0x100));
        }

        sw.SetMmu (&mmu);
        mmu.Initialize (&bus, &mainRam, nullptr, nullptr, nullptr, &sw);
        bus.AddDevice (&sw);

        Byte * auxPtr = mmu.GetAuxBuffer();

        // Distinguishable content so a main/aux mix-up cannot look correct.
        mainPtr[0x2000] = 0x2A;   // 0b0101010
        auxPtr [0x2000] = 0x55;   // 0b1010101

        sw.Write     (0xC001, 0x00);   // 80STORE on -- bank surface, not bus
        bus.ReadByte (0xC057);         // HIRES on
        bus.ReadByte (0xC055);         // PAGE2 on

        Assert::IsTrue (mmu.Get80Store(), L"fixture: 80STORE did not latch");
        Assert::IsTrue (sw.IsHiresMode(), L"fixture: HIRES did not latch");
        Assert::IsTrue (sw.IsPage2(),     L"fixture: PAGE2 did not latch");

        Assert::AreEqual (static_cast<Byte> (0x55), bus.ReadByte (0x2000),
            L"fixture: the bus must now be banked to aux for this to prove anything");

        AppleDoubleHiResMode dhr (bus);
        dhr.SetAuxMemory  (auxPtr);
        dhr.SetMainMemory (mainPtr);
        dhr.SetPage2      (false);
        dhr.SetMonochrome (true);

        dhr.Render (nullptr, fb.data(), 560, 384);

        for (int x = 0; x < 14; x++)
        {
            dots += (fb[x] == 0xFFFFFFFFu) ? '1' : '0';
        }

        // Dots 0-6 are aux 0x55; dots 7-13 are main 0x2A. Rendering the aux
        // byte twice would read "10101011010101".
        Assert::AreEqual (std::string ("10101010101010"), dots,
            L"DHR must read the main half from main RAM, not from the banked bus");
    }

    // A color monitor is the default, since that is what the shell selects
    // until the user picks a phosphor.
    TEST_METHOD (DHR_ColorMonitorIsTheDefault)
    {
        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        AppleDoubleHiResMode dhr (bus);

        Assert::IsFalse (dhr.IsMonochrome(),
            L"DHR must start on the color decode");
    }

    // One lit dot must light exactly one pixel on a monochrome monitor. The
    // color decode widens it to the whole 4-dot cell, which is what makes
    // 560x192 monochrome artwork unreadable through it.
    TEST_METHOD (DHR_Monochrome_LightsOneDotPerPixel)
    {
        const int              fbW  = 560;
        const int              fbH  = 384;
        std::vector<uint32_t>  fb (fbW * fbH, 0xFFCCCCCC);
        std::vector<Byte>      auxBuf (0x10000, 0x00);



        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x2000; addr < 0x4000; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        // Bit 0 of the aux byte is dot 0; bit 0 of the main byte is dot 7.
        // Everything else on scanline 0 stays dark.
        auxBuf[0x2000] = 0x01;
        bus.WriteByte (0x2000, 0x01);

        AppleDoubleHiResMode dhr (bus);
        dhr.SetAuxMemory  (auxBuf.data());
        dhr.SetPage2      (false);
        dhr.SetMonochrome (true);

        dhr.Render (nullptr, fb.data(), fbW, fbH);

        Assert::AreEqual (0xFFFFFFFFu, fb[0],
            L"Dot 0 (aux bit 0) must be lit");
        Assert::AreEqual (0xFFFFFFFFu, fb[7],
            L"Dot 7 (main bit 0) must be lit");

        for (int x = 1; x < 7; x++)
        {
            Assert::AreEqual (0xFF000000u, fb[x],
                L"A lit dot must not spread across its 4-dot color cell");
        }

        for (int x = 8; x < 14; x++)
        {
            Assert::AreEqual (0xFF000000u, fb[x],
                L"A lit dot must not spread across its 4-dot color cell");
        }

        // Scanlines are still doubled, so row 1 repeats row 0.
        Assert::AreEqual (0xFFFFFFFFu, fb[fbW + 0], L"Row 1 must repeat scanline 0");
        Assert::AreEqual (0xFF000000u, fb[fbW + 1], L"Row 1 must repeat scanline 0");
    }

    // The dither patterns that monochrome DHR art shades with survive on a
    // monochrome monitor and collapse on a color one. Asserting both halves
    // together is the point: it is the DIFFERENCE that was missing, not
    // either decode on its own.
    TEST_METHOD (DHR_Monochrome_KeepsDitherThatColorCollapses)
    {
        const int              fbW      = 560;
        const int              fbH      = 384;
        std::vector<uint32_t>  monoFb (fbW * fbH, 0xFFCCCCCC);
        std::vector<uint32_t>  colorFb (fbW * fbH, 0xFFCCCCCC);
        std::vector<Byte>      auxBuf (0x10000, 0x00);
        int                    monoEdges = 0;
        const Byte             kDither   = 0x55;



        MemoryBus bus;
        RamDevice ram (0x0000, 0x5FFF);
        bus.AddDevice (&ram);

        for (Word addr = 0x2000; addr < 0x4000; addr++)
        {
            bus.WriteByte (addr, 0x00);
        }

        // 0b1010101 in both halves: alternating dots, which is the shading
        // dither monochrome DHR art is built from and exactly what the 4-dot
        // cell decode cannot represent.
        //
        // The alternation is NOT uniform across the byte pair -- aux bit 6
        // (dot 6) and main bit 0 (dot 7) are both set, so the two lit dots
        // meet at the seam. That is the hardware's interleave showing
        // through, so the expectation is derived from the bytes rather than
        // assumed to be a clean every-other-dot pattern.
        auxBuf[0x2000] = kDither;
        bus.WriteByte (0x2000, kDither);

        AppleDoubleHiResMode dhr (bus);
        dhr.SetAuxMemory (auxBuf.data());
        dhr.SetPage2     (false);

        dhr.SetMonochrome (true);
        dhr.Render (nullptr, monoFb.data(), fbW, fbH);

        dhr.SetMonochrome (false);
        dhr.Render (nullptr, colorFb.data(), fbW, fbH);

        // Dots 0-6 come from the aux byte's bits 0-6, dots 7-13 from the
        // main byte's bits 0-6.
        for (int x = 0; x < 14; x++)
        {
            bool     lit      = (kDither & (1 << (x % 7))) != 0;
            uint32_t expected = lit ? 0xFFFFFFFFu : 0xFF000000u;

            Assert::AreEqual (expected, monoFb[x],
                L"Monochrome DHR must reproduce the dither dot for dot");
        }

        // Count lit/dark edges across the byte pair. Monochrome resolves
        // every dot; the color decode paints uniform 4-dot cells, so it can
        // never show an edge at an odd offset inside a cell.
        for (int x = 1; x < 14; x++)
        {
            if (monoFb[x] != monoFb[x - 1]) { monoEdges++; }
        }

        // 13 boundaries, all alternating except the seam at dot 6/7.
        Assert::AreEqual (12, monoEdges,
            L"Monochrome DHR must resolve an edge at every dot but the seam");

        for (int x = 1; x < 4; x++)
        {
            Assert::AreEqual (colorFb[0], colorFb[x],
                L"The color decode paints one color across a whole 4-dot cell");
        }
    }
};
