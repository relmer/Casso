#include "Pch.h"

#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiNoticeStackTests
//
//  Notices stacked in arrival order, each up for its own full time, the ones
//  below sliding up when one above expires. Every test passes the time and
//  sets the animation flag itself; none reads the clock or the system's
//  animation setting, which the CI runner reports as off.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiNoticeStackTests
{
    static constexpr int64_t  kDurationMs  = 4000;
    static constexpr int64_t  kApartMs     = 1000;
    static constexpr float    kWidthPx     = 800.0f;
    static constexpr float    kTolerancePx = 0.001f;


    TEST_CLASS (DxuiNoticeStackTests)
    {
    public:

        TEST_METHOD (TwoNoticesASecondApartEachStayUpForTheirOwnFullDuration)
        {
            DxuiNoticeStack  stack;



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"First", 0);
            stack.Push (L"Second", kApartMs);

            Assert::AreEqual ((size_t) 2, stack.GetCount(), L"the second notice is added, not swapped in");

            stack.Tick (kDurationMs - 1);
            Assert::AreEqual ((size_t) 2, stack.GetCount(), L"the first is still inside its time");
            Assert::AreEqual (std::wstring (L"First"), stack.GetText (0));

            stack.Tick (kDurationMs);
            Assert::AreEqual ((size_t) 1, stack.GetCount(), L"the first leaves at its own expiry");
            Assert::AreEqual (std::wstring (L"Second"), stack.GetText (0));

            stack.Tick (kApartMs + kDurationMs - 1);
            Assert::AreEqual ((size_t) 1, stack.GetCount(), L"the second keeps its full time");
            Assert::IsTrue (stack.IsShowing (kApartMs + kDurationMs - 1));

            stack.Tick (kApartMs + kDurationMs);
            Assert::AreEqual ((size_t) 0, stack.GetCount());
            Assert::IsFalse (stack.IsShowing (kApartMs + kDurationMs));
            Assert::IsFalse (stack.IsVisible());
        }



        TEST_METHOD (TheSecondAppearsBelowTheFirstWithoutASlide)
        {
            DxuiNoticeStack  stack;
            float            firstHeightPx = 0.0f;



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"First", 0);
            firstHeightPx = Measure (stack);

            stack.Push (L"Second", kApartMs);
            Measure (stack);

            Assert::AreEqual (0.0f, stack.GetOffsetPx (0, kApartMs), kTolerancePx);
            Assert::AreEqual (firstHeightPx + GetGapPx(), stack.GetOffsetPx (1, kApartMs), kTolerancePx,
                              L"the second sits under the first and the gap");
            Assert::IsFalse (stack.IsAnimating (kApartMs), L"an arrival does not slide");
        }



        TEST_METHOD (TheFirstExpiringSlidesTheSecondToTheTop)
        {
            constexpr int64_t  kHalfMs  = DxuiPopupMenu::kRevealMs / 2;
            constexpr float    kHalfway = 0.5f;

            DxuiNoticeStack  stack;
            float            distancePx = 0.0f;



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"First", 0);
            stack.Push (L"Second", kApartMs);
            Measure (stack);

            distancePx = stack.GetOffsetPx (1, kDurationMs - 1);

            stack.Tick (kDurationMs);

            Assert::AreEqual (distancePx, stack.GetOffsetPx (0, kDurationMs), kTolerancePx,
                              L"the slide starts where the second was");
            Assert::IsTrue (stack.IsAnimating (kDurationMs));

            Assert::AreEqual (distancePx * (1.0f - DxuiAnimation::ApplyEase (DxuiTweenEase::EaseOut, kHalfway)),
                              stack.GetOffsetPx (0, kDurationMs + kHalfMs),
                              kTolerancePx,
                              L"midway along the ease-out curve");

            Assert::AreEqual (0.0f, stack.GetOffsetPx (0, kDurationMs + DxuiPopupMenu::kRevealMs), kTolerancePx,
                              L"at the top once the menus' open time has run");
            Assert::IsFalse (stack.IsAnimating (kDurationMs + DxuiPopupMenu::kRevealMs));
        }



        TEST_METHOD (WithAnimationsOffTheSecondMovesToTheTopAtOnce)
        {
            DxuiNoticeStack  stack;



            stack.SetAnimationsEnabled (false);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"First", 0);
            stack.Push (L"Second", kApartMs);
            Measure (stack);

            stack.Tick (kDurationMs);

            Assert::AreEqual (0.0f, stack.GetOffsetPx (0, kDurationMs), kTolerancePx);
            Assert::IsFalse (stack.IsAnimating (kDurationMs));
        }



        TEST_METHOD (TheMiddleOfThreeExpiringFirstMovesOnlyTheBottom)
        {
            constexpr int64_t  kShortMs  = 1000;
            constexpr int64_t  kMiddleAt = 100;
            constexpr int64_t  kBottomAt = 200;
            constexpr int64_t  kExpiryMs = kMiddleAt + kShortMs;
            constexpr int64_t  kHalfMs   = DxuiPopupMenu::kRevealMs / 2;

            DxuiNoticeStack  stack;
            float            topHeightPx = 0.0f;
            float            bottomWasPx = 0.0f;



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"Top", 0);
            stack.SetDurationMs (kShortMs);
            stack.Push (L"Middle", kMiddleAt);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"Bottom", kBottomAt);
            Measure (stack);

            topHeightPx = stack.GetOffsetPx (1, kBottomAt) - GetGapPx();
            bottomWasPx = stack.GetOffsetPx (2, kExpiryMs - 1);

            stack.Tick (kExpiryMs);

            Assert::AreEqual ((size_t) 2, stack.GetCount());
            Assert::AreEqual (std::wstring (L"Top"),    stack.GetText (0));
            Assert::AreEqual (std::wstring (L"Bottom"), stack.GetText (1));

            Assert::AreEqual (0.0f, stack.GetOffsetPx (0, kExpiryMs),           kTolerancePx, L"the top stays");
            Assert::AreEqual (0.0f, stack.GetOffsetPx (0, kExpiryMs + kHalfMs), kTolerancePx, L"the top stays");

            Assert::AreEqual (bottomWasPx, stack.GetOffsetPx (1, kExpiryMs), kTolerancePx,
                              L"the bottom starts where it was");
            Assert::IsTrue (stack.GetOffsetPx (1, kExpiryMs + kHalfMs) < bottomWasPx, L"the bottom moves up");
            Assert::AreEqual (topHeightPx + GetGapPx(), stack.GetOffsetPx (1, kExpiryMs + DxuiPopupMenu::kRevealMs),
                              kTolerancePx, L"the bottom ends right under the top");
        }



        TEST_METHOD (ASecondExpiryMidSlideContinuesFromTheCurrentOffset)
        {
            constexpr int64_t  kSecondAt = 50;
            constexpr int64_t  kThirdAt  = 100;
            constexpr int64_t  kMidMs    = kDurationMs + kSecondAt;

            DxuiNoticeStack  stack;
            float            thirdWasPx = 0.0f;



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"First", 0);
            stack.Push (L"Second", kSecondAt);
            stack.Push (L"Third", kThirdAt);
            Measure (stack);

            stack.Tick (kDurationMs);
            Assert::IsTrue (stack.IsAnimating (kMidMs), L"the second expiry lands mid-slide");

            thirdWasPx = stack.GetOffsetPx (1, kMidMs);

            stack.Tick (kMidMs);

            Assert::AreEqual ((size_t) 1, stack.GetCount());
            Assert::AreEqual (thirdWasPx, stack.GetOffsetPx (0, kMidMs), kTolerancePx,
                              L"the new slide starts from where the notice was, not from its old place");
            Assert::AreEqual (0.0f, stack.GetOffsetPx (0, kMidMs + DxuiPopupMenu::kRevealMs), kTolerancePx);
        }



        TEST_METHOD (ANoticeArrivingMidSlideMovesWithTheOneAboveIt)
        {
            constexpr int64_t  kArrivalMs = kDurationMs + DxuiPopupMenu::kRevealMs / 2;

            DxuiNoticeStack  stack;
            float            secondPx = 0.0f;
            float            thirdPx  = 0.0f;



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"First", 0);
            stack.Push (L"Second", kApartMs);
            Measure (stack);

            stack.Tick (kDurationMs);
            stack.Push (L"Third", kArrivalMs);
            Measure (stack);

            secondPx = stack.GetOffsetPx (0, kArrivalMs);
            thirdPx  = stack.GetOffsetPx (1, kArrivalMs);

            Assert::IsTrue (thirdPx > secondPx, L"the arrival is below the one still sliding");
            Assert::AreEqual (stack.GetOffsetPx (1, kDurationMs + DxuiPopupMenu::kRevealMs) -
                              stack.GetOffsetPx (0, kDurationMs + DxuiPopupMenu::kRevealMs),
                              thirdPx - secondPx,
                              kTolerancePx,
                              L"the two keep their spacing through the slide");
        }



        TEST_METHOD (GetNextChangeMsGivesTheNearestExpiryOrSlideEnd)
        {
            DxuiNoticeStack  stack;



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);

            Assert::IsFalse (stack.GetNextChangeMs().has_value(), L"nothing when empty");

            stack.Push (L"First", 0);
            stack.Push (L"Second", kApartMs);
            Measure (stack);
            Assert::AreEqual (kDurationMs, stack.GetNextChangeMs().value(), L"the first expiry");

            stack.Tick (kDurationMs);
            Assert::AreEqual (kDurationMs + DxuiPopupMenu::kRevealMs, stack.GetNextChangeMs().value(),
                              L"the slide ends before the second expires");

            stack.Tick (kDurationMs + DxuiPopupMenu::kRevealMs);
            Assert::AreEqual (kApartMs + kDurationMs, stack.GetNextChangeMs().value(), L"the second expiry");

            stack.Tick (kApartMs + kDurationMs);
            Assert::IsFalse (stack.GetNextChangeMs().has_value(), L"nothing once the last has gone");
        }



        TEST_METHOD (ClearTakesEveryNoticeDown)
        {
            DxuiNoticeStack  stack;



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"First", 0);
            stack.Push (L"Second", kApartMs);

            stack.Clear();

            Assert::AreEqual ((size_t) 0, stack.GetCount());
            Assert::IsFalse (stack.IsShowing (kApartMs));
            Assert::IsFalse (stack.IsVisible());
        }



        TEST_METHOD (EachNoticeKeepsTheBannerLabelRoleAndText)
        {
            DxuiNoticeStack  stack;



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"First", 0);
            stack.Push (L"Second", kApartMs);

            Assert::AreEqual ((size_t) 2, stack.GetChildCount());
            Assert::IsTrue (stack.GetChild (1)->GetAccessibleRole() == DxuiAccessibleRole::Label);
            Assert::AreEqual (std::wstring (L"Second"), stack.GetChild (1)->GetAccessibleName());
        }



        TEST_METHOD (LayoutHangsEachNoticeFromTheTopOfTheBounds)
        {
            constexpr LONG  kTop = 40;

            DxuiNoticeStack  stack;
            DxuiDpiScaler    scaler;
            RECT             bounds = { 0, kTop, (LONG) kWidthPx, kTop };
            RECT             first  = {};
            RECT             second = {};



            stack.SetAnimationsEnabled (true);
            stack.SetDurationMs (kDurationMs);
            stack.Push (L"First", 0);
            stack.Push (L"Second", kApartMs);
            bounds.bottom += (LONG) Measure (stack);

            stack.Layout (bounds, scaler);
            first  = stack.GetChild (0)->GetBounds();
            second = stack.GetChild (1)->GetBounds();

            Assert::AreEqual (kTop, first.top);
            Assert::AreEqual (bounds.right, first.right);
            Assert::AreEqual (kTop + (LONG) stack.GetOffsetPx (1, kApartMs), second.top);
            Assert::IsTrue (second.top > first.bottom, L"the gap separates the two");
            Assert::AreEqual (bounds.bottom, second.bottom, L"the measured height covers the whole stack");
        }

    private:

        //  Measures at 96 DPI with the recording renderer, and returns the
        //  height the stack covers.
        static float Measure (DxuiNoticeStack & stack)
        {
            MockDxuiTextRenderer  text;
            DxuiDpiScaler         scaler;
            float                 totalPx = 0.0f;



            totalPx = stack.MeasureHeightPx (&text, kWidthPx, scaler);
            Assert::IsTrue (totalPx > 0.0f, L"the stack has a height to measure");

            return totalPx;
        }


        static float GetGapPx()
        {
            return (float) DxuiNoticeStack::kGapDip;
        }
    };
}
