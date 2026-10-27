#include "Pch.h"

#include "Ui/Chrome/RewindBar.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





namespace RewindBarBandTests
{
    static constexpr RECT  kScreen    = { 100, 50, 660, 434 };
    static constexpr UINT  kDpi96     = 96;
    static constexpr UINT  kDpi144    = 144;
    static constexpr LONG  kMiddleX   = 380;





    ////////////////////////////////////////////////////////////////////////////////
    //
    //  RewindBarBandTests
    //
    //  The rule that opens the rewind bar: the pointer in the bottom 8 DIPs
    //  of the emulated screen, scaled for the DPI, and not above them or
    //  outside the screen; never while the pointer is captured, or with
    //  history off.
    //
    ////////////////////////////////////////////////////////////////////////////////

    TEST_CLASS (RewindBarBandTests)
    {
    public:

        TEST_METHOD (TheBottomEightDipsOpenTheBar)
        {
            Assert::IsTrue  (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom - 1 }, kScreen, kDpi96, false, true), L"the bottom row");
            Assert::IsTrue  (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom - 8 }, kScreen, kDpi96, false, true), L"the band's top row");
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom - 9 }, kScreen, kDpi96, false, true), L"the row above it");
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom },     kScreen, kDpi96, false, true), L"the row below the screen");
        }


        TEST_METHOD (TheBandScalesWithTheDpi)
        {
            Assert::IsTrue  (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom - 12 }, kScreen, kDpi144, false, true), L"12 pixels at 150%");
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom - 13 }, kScreen, kDpi144, false, true), L"and no more");
        }


        TEST_METHOD (OutsideTheScreenOpensNothing)
        {
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kScreen.left - 1, kScreen.bottom - 1 }, kScreen, kDpi96, false, true), L"left of it");
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kScreen.right,    kScreen.bottom - 1 }, kScreen, kDpi96, false, true), L"right of it");
            Assert::IsTrue  (RewindBar::IsInOpeningBand ({ kScreen.left,     kScreen.bottom - 1 }, kScreen, kDpi96, false, true), L"its left column");
        }


        TEST_METHOD (OnACurvedScreenTheBandIsTheBottomOfTheGlassUnderThePointer)
        {
            //  A screen whose bottom edge bows down 20 pixels in the middle.
            RewindBar::ScreenTestFn  isOnScreen = [] (int x, int y)
            {
                LONG  bow = 20 - std::abs (x - kMiddleX) / 14;

                return x >= kScreen.left && x < kScreen.right && y >= kScreen.top && y < kScreen.bottom + bow;
            };



            Assert::IsTrue  (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom + 19 }, isOnScreen, kDpi96, false, true), L"the bottom row in the middle");
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom + 11 }, isOnScreen, kDpi96, false, true), L"above the band in the middle");
            Assert::IsTrue  (RewindBar::IsInOpeningBand ({ kScreen.left, kScreen.bottom - 1 }, isOnScreen, kDpi96, false, true), L"the bottom row at the edge, higher up");
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom + 19 }, isOnScreen, kDpi96, true,  true), L"captured");
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom + 19 }, isOnScreen, kDpi96, false, false), L"history off");
        }


        TEST_METHOD (TheScreenIsFoundAroundAPointOnIt)
        {
            RewindBar::ScreenTestFn  isOnScreen = [] (int x, int y) { return x >= kScreen.left && x < kScreen.right && y >= kScreen.top && y < kScreen.bottom; };
            RECT                     found      = RewindBar::FindScreen ({ kMiddleX, kScreen.bottom - 5 }, isOnScreen);



            Assert::IsTrue (EqualRect (&found, &kScreen) != FALSE, L"the screen's rectangle");
        }


        TEST_METHOD (ACapturedPointerOrHistoryOffOpensNothing)
        {
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom - 1 }, kScreen, kDpi96, true,  true),  L"captured");
            Assert::IsFalse (RewindBar::IsInOpeningBand ({ kMiddleX, kScreen.bottom - 1 }, kScreen, kDpi96, false, false), L"history off");
        }
    };
}
