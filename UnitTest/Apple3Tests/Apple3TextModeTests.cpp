#include "Pch.h"

#include "Machines/Apple3/Apple380ColTextMode.h"
#include "Machines/Apple3/Apple3ColorTextMode.h"
#include "Machines/Apple3/Apple3Screen.h"
#include "Machines/Apple3/Apple3TextCells.h"
#include "Machines/Apple3/Apple3TextMode.h"
#include "Machines/Apple3/IGlyphSource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  FakeGlyphs
//
////////////////////////////////////////////////////////////////////////////////

class FakeGlyphs : public IGlyphSource
{
public:
    Byte  GetGlyphRow (Byte code, int row) const override { return m_rows[(size_t) (code & 0x7F) * kRowCount + (size_t) row]; }
    void  SetRow      (Byte code, int row, Byte dots)     { m_rows[(size_t) code * kRowCount + (size_t) row] = dots; }

private:
    std::array<Byte, (size_t) kCharacterCount * kRowCount>  m_rows = {};
};





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3TextModeTests
//
//  The three text modes' decoding: the row layout, normal and inverse
//  characters, flashing in the 40-column black and white mode, the color
//  byte, the 80-column interleave and the page switch.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (Apple3TextModeTests)
{
public:

    static constexpr Byte  kCodeA     = 0x01;
    static constexpr Byte  kCodeB     = 0x02;
    static constexpr Byte  kNormalBit = 0x80;
    static constexpr Byte  kFlashBit  = 0x80;

    std::array<Byte, 0x2000>    m_ram    = {};
    std::function<Byte (Word)>  m_reader = [this] (Word address) { return m_ram[address]; };
    FakeGlyphs                  m_glyphs;
    Apple3Screen                m_screen;


    //  Character A has its first dot lit on its first line; B has its
    //  second dot lit.
    TEST_METHOD_INITIALIZE (SetUp)
    {
        m_glyphs.SetRow (kCodeA, 0, 0x01);
        m_glyphs.SetRow (kCodeB, 0, 0x02);
    }


    void Decode (const IApple3VideoMode & mode, bool isPage2, bool isFlashOn)
    {
        Apple3VideoInput  input { m_reader, m_glyphs, isPage2, isFlashOn };



        mode.Decode (input, m_screen);
    }


    TEST_METHOD (RowAddresses_FollowTheApple2Layout)
    {
        Assert::AreEqual ((Word) 0x0400, Apple3TextCells::GetRowAddress (0,  Apple3TextCells::kPage1));
        Assert::AreEqual ((Word) 0x0480, Apple3TextCells::GetRowAddress (1,  Apple3TextCells::kPage1));
        Assert::AreEqual ((Word) 0x0428, Apple3TextCells::GetRowAddress (8,  Apple3TextCells::kPage1));
        Assert::AreEqual ((Word) 0x07D0, Apple3TextCells::GetRowAddress (23, Apple3TextCells::kPage1));
        Assert::AreEqual ((Word) 0x0BD0, Apple3TextCells::GetRowAddress (23, Apple3TextCells::kPage2));
    }


    TEST_METHOD (Text40_NormalCharacter_LightsItsDotsTwoPixelsWide)
    {
        m_ram[0x0400] = kCodeA | kNormalBit;

        Decode (Apple3TextMode(), false, false);

        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (0, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (1, 0));
        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (2, 0));
    }


    TEST_METHOD (Text40_InverseCharacter_SwapsTheDots)
    {
        m_ram[0x0400] = kCodeA;

        Decode (Apple3TextMode(), false, false);

        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (0, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (2, 0));
    }


    TEST_METHOD (Text40_FlashingRow_IsInverseOnlyInTheOnPhase)
    {
        m_ram[0x0400] = kCodeA;
        m_glyphs.SetRow (kCodeA, 0, 0x01 | kFlashBit);

        Decode (Apple3TextMode(), false, true);
        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (0, 0));

        Decode (Apple3TextMode(), false, false);
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (0, 0));
    }


    TEST_METHOD (Text40_SteadyInverseRow_IsInverseInBothPhases)
    {
        m_ram[0x0400] = kCodeA;

        Decode (Apple3TextMode(), false, true);
        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (0, 0));

        Decode (Apple3TextMode(), false, false);
        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (0, 0));
    }


    TEST_METHOD (Text40_Page2_ReadsTheSecondPage)
    {
        m_ram[0x0400] = kCodeA | kNormalBit;
        m_ram[0x0800] = kCodeB | kNormalBit;

        Decode (Apple3TextMode(), true, false);

        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (0, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (2, 0));
    }


    TEST_METHOD (Text40_SecondColumn_StartsFourteenPixelsIn)
    {
        m_ram[0x0400] = kNormalBit;
        m_ram[0x0401] = kCodeA | kNormalBit;

        Decode (Apple3TextMode(), false, false);

        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (13, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (14, 0));
    }


    TEST_METHOD (ColorText_UsesTheColorByteFromTheOtherPage)
    {
        m_ram[0x0400] = kCodeA | kNormalBit;
        m_ram[0x0800] = 0x5A;

        Decode (Apple3ColorTextMode(), false, false);

        Assert::AreEqual ((Byte) 0x5, m_screen.Get (0, 0));
        Assert::AreEqual ((Byte) 0xA, m_screen.Get (2, 0));
    }


    TEST_METHOD (ColorText_Page2_SwapsCharactersAndColors)
    {
        m_ram[0x0800] = kCodeA | kNormalBit;
        m_ram[0x0400] = 0x3C;

        Decode (Apple3ColorTextMode(), true, false);

        Assert::AreEqual ((Byte) 0x3, m_screen.Get (0, 0));
        Assert::AreEqual ((Byte) 0xC, m_screen.Get (2, 0));
    }


    TEST_METHOD (ColorText_InverseSwapsTheColors_AndAFlashingRowIsSteady)
    {
        m_ram[0x0400] = kCodeA;
        m_ram[0x0800] = 0x5A;
        m_glyphs.SetRow (kCodeA, 0, 0x01 | kFlashBit);

        Decode (Apple3ColorTextMode(), false, false);

        Assert::AreEqual ((Byte) 0xA, m_screen.Get (0, 0));
        Assert::AreEqual ((Byte) 0x5, m_screen.Get (2, 0));
    }


    TEST_METHOD (Text80_TakesEvenColumnsFromPage1AndOddFromPage2)
    {
        m_ram[0x0400] = kCodeA | kNormalBit;
        m_ram[0x0800] = kCodeB | kNormalBit;

        Decode (Apple380ColTextMode(), false, false);

        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (0, 0));
        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (1, 0));
        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (7, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (8, 0));
    }


    TEST_METHOD (Text80_Page2_SwapsTheColumns)
    {
        m_ram[0x0400] = kCodeA | kNormalBit;
        m_ram[0x0800] = kCodeB | kNormalBit;

        Decode (Apple380ColTextMode(), true, false);

        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (0, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (1, 0));
        Assert::AreEqual (Apple3Screen::kWhite, m_screen.Get (7, 0));
    }


    TEST_METHOD (Text80_FlashingRow_IsSteadyInverse)
    {
        m_ram[0x0400] = kCodeA;
        m_glyphs.SetRow (kCodeA, 0, 0x01 | kFlashBit);

        Decode (Apple380ColTextMode(), false, false);

        Assert::AreEqual (Apple3Screen::kBlack, m_screen.Get (0, 0));
    }
};
