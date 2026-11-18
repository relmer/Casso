#include "Pch.h"

#include "Machines/Apple2/Common/NtscColorTable.h"
#include "Machines/Apple3/Apple3Color140Mode.h"
#include "Machines/Apple3/Apple3ColorHiResMode.h"
#include "Machines/Apple3/Apple3GraphicsRows.h"
#include "Machines/Apple3/Apple3HiRes560Mode.h"
#include "Machines/Apple3/Apple3HiResMode.h"
#include "Machines/Apple3/Apple3Palette.h"
#include "Machines/Apple3/Apple3Screen.h"
#include "Machines/Apple3/IGlyphSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  NoGlyphs
//
////////////////////////////////////////////////////////////////////////////////

class NoGlyphs : public IGlyphSource
{
public:
    Byte  GetGlyphRow (Byte code, int row) const override { UNREFERENCED_PARAMETER (code); UNREFERENCED_PARAMETER (row); return 0; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3GraphicsModeTests
//
//  The four graphics modes' decoding from bank 0: the line layout, smooth
//  scroll, each mode's dots and colors, the page switch, and the palette's
//  colors and gray levels.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3GraphicsModeTests)
{
public:

    static constexpr Word  kBank0Base = 0x2000;

    std::array<Byte, 0x8000>    m_bank0  = {};
    std::function<Byte (Word)>  m_system = [] (Word) { return (Byte) 0; };
    std::function<Byte (Word)>  m_reader = [this] (Word address) { return m_bank0[(size_t) (address - kBank0Base)]; };
    NoGlyphs                    m_glyphs;
    Apple3Screen                m_screen;


    void Poke (Word address, Byte value) { m_bank0[(size_t) (address - kBank0Base)] = value; }


    void Decode (const IApple3VideoMode & mode, bool isPage2, int scrollOffset = 0)
    {
        Apple3VideoInput  input { m_system, m_glyphs, isPage2, false, &m_reader, scrollOffset };



        mode.Decode (input, m_screen);
    }


    TEST_METHOD (LineAddresses_FollowTheApple2HiResLayout)
    {
        Assert::AreEqual ((Word) 0x2000, Apple3GraphicsRows::GetLineAddress (0,   0x2000, 0));
        Assert::AreEqual ((Word) 0x2400, Apple3GraphicsRows::GetLineAddress (1,   0x2000, 0));
        Assert::AreEqual ((Word) 0x2080, Apple3GraphicsRows::GetLineAddress (8,   0x2000, 0));
        Assert::AreEqual ((Word) 0x2028, Apple3GraphicsRows::GetLineAddress (64,  0x2000, 0));
        Assert::AreEqual ((Word) 0x3FD0, Apple3GraphicsRows::GetLineAddress (191, 0x2000, 0));
    }


    TEST_METHOD (SmoothScroll_MovesEachLineWithinItsGroup)
    {
        Assert::AreEqual ((Word) 0x2400, Apple3GraphicsRows::GetLineAddress (0, 0x2000, 1));
        Assert::AreEqual ((Word) 0x2000, Apple3GraphicsRows::GetLineAddress (7, 0x2000, 1));
        Assert::AreEqual ((Word) 0x2480, Apple3GraphicsRows::GetLineAddress (8, 0x2000, 1));
    }


    TEST_METHOD (HiRes_EachDotIsTwoPixels_AndPage2IsBuffer2)
    {
        Poke (0x2000, 0x01);
        Poke (0x4000, 0x02);

        Decode (Apple3HiResMode(), false);
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (0, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (1, 0));
        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (2, 0));

        Decode (Apple3HiResMode(), true);
        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (0, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (2, 0));
    }


    TEST_METHOD (ColorHiRes_TakesTheColorByteFromTheOtherBuffer)
    {
        Poke (0x2000, 0x01);
        Poke (0x4000, 0x5A);

        Decode (Apple3ColorHiResMode(), false);
        Assert::AreEqual ((Byte) 0x5, m_screen.Get (0, 0));
        Assert::AreEqual ((Byte) 0xA, m_screen.Get (2, 0));
    }


    TEST_METHOD (ColorHiRes_Page2_SwapsDotsAndColors)
    {
        Poke (0x4000, 0x01);
        Poke (0x2000, 0x3C);

        Decode (Apple3ColorHiResMode(), true);
        Assert::AreEqual ((Byte) 0x3, m_screen.Get (0, 0));
        Assert::AreEqual ((Byte) 0xC, m_screen.Get (2, 0));
    }


    TEST_METHOD (HiRes560_AlternatesBuffers_APixelPerDot)
    {
        Poke (0x2000, 0x01);
        Poke (0x4000, 0x01);

        Decode (Apple3HiRes560Mode(), false);
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (0, 0));
        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (1, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (7, 0));
    }


    TEST_METHOD (HiRes560_Page2_IsBuffers3And4)
    {
        Poke (0x6000, 0x02);

        Decode (Apple3HiRes560Mode(), true);
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (1, 0));
    }


    TEST_METHOD (Color140_FourBytesAreSevenPixelsOfFourBits)
    {
        Poke (0x2000, 0x35);
        Poke (0x4000, 0x01);

        Decode (Apple3Color140Mode(), false);
        Assert::AreEqual ((Byte) 0x5, m_screen.Get (0, 0));
        Assert::AreEqual ((Byte) 0x5, m_screen.Get (3, 0));
        Assert::AreEqual ((Byte) 0xB, m_screen.Get (4, 0), L"bits 4-6 of the first byte and bit 0 of the second");
    }


    TEST_METHOD (Palette_ColorIsTheLoResSet_AndMonochromeIsTheGrayLevel)
    {
        Assert::AreEqual (kAppleColors[9], Apple3Palette::GetPixel (9, false));
        Assert::AreEqual (0xFF555555u,     Apple3Palette::GetPixel (5, true));
        Assert::AreEqual (0xFF000000u,     Apple3Palette::GetPixel (0, true));
        Assert::AreEqual (0xFFFFFFFFu,     Apple3Palette::GetPixel (15, true));
    }
};