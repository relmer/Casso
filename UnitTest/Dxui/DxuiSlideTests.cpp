#include "Pch.h"

#include "Core/DxuiSlide.h"
#include "Core/DxuiAnimation.h"
#include "Widgets/DxuiPopupMenu.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSlideTests
//
//  A distance run down to zero over the time and curve a menu opens with.
//  Every slide here takes its time and its animation flag as arguments, so
//  none of them reads the clock or the system's animation setting.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiSlideTests
{
    static constexpr float    kDistancePx  = 100.0f;
    static constexpr int64_t  kStartMs     = 1000;
    static constexpr float    kTolerancePx = 0.001f;


    TEST_CLASS (DxuiSlideTests)
    {
    public:

        TEST_METHOD (TheOffsetAtTheStartIsTheWholeDistance)
        {
            DxuiSlide  slide = DxuiSlide::Start (kDistancePx, kStartMs, true);



            Assert::AreEqual (kDistancePx, slide.GetOffset (kStartMs), kTolerancePx);
            Assert::IsFalse (slide.IsDone (kStartMs));
        }



        TEST_METHOD (TheOffsetAtTheMidpointFollowsTheEaseOutCurve)
        {
            constexpr int64_t  kHalfMs  = DxuiPopupMenu::kRevealMs / 2;
            constexpr float    kHalfway = 0.5f;

            DxuiSlide  slide    = DxuiSlide::Start (kDistancePx, kStartMs, true);
            float      expected = kDistancePx * (1.0f - DxuiAnimation::ApplyEase (DxuiTweenEase::EaseOut, kHalfway));



            Assert::AreEqual (expected, slide.GetOffset (kStartMs + kHalfMs), kTolerancePx);
        }



        TEST_METHOD (TheOffsetIsZeroAtTheRevealTimeAndAfter)
        {
            constexpr int64_t  kLaterMs = 1000;

            DxuiSlide  slide = DxuiSlide::Start (kDistancePx, kStartMs, true);



            Assert::AreEqual (0.0f, slide.GetOffset (kStartMs + DxuiPopupMenu::kRevealMs), kTolerancePx);
            Assert::IsTrue (slide.IsDone (kStartMs + DxuiPopupMenu::kRevealMs));

            Assert::AreEqual (0.0f, slide.GetOffset (kStartMs + DxuiPopupMenu::kRevealMs + kLaterMs), kTolerancePx);
            Assert::IsTrue (slide.IsDone (kStartMs + DxuiPopupMenu::kRevealMs + kLaterMs));
            Assert::AreEqual (kStartMs + DxuiPopupMenu::kRevealMs, slide.GetEndMs());
        }



        TEST_METHOD (WithAnimationsOffTheSlideIsDoneAtOnce)
        {
            DxuiSlide  slide = DxuiSlide::Start (kDistancePx, kStartMs, false);



            Assert::IsTrue (slide.IsDone (kStartMs));
            Assert::AreEqual (0.0f, slide.GetOffset (kStartMs), kTolerancePx);
            Assert::AreEqual (kStartMs, slide.GetEndMs());
        }



        TEST_METHOD (ASlideStartedFromANonzeroOffsetContinuesFromIt)
        {
            constexpr int64_t  kMidMs     = kStartMs + DxuiPopupMenu::kRevealMs / 2;
            constexpr float    kFurtherPx = 40.0f;

            DxuiSlide  first     = DxuiSlide::Start (kDistancePx, kStartMs, true);
            float      currentPx = first.GetOffset (kMidMs);
            DxuiSlide  second    = DxuiSlide::Start (currentPx + kFurtherPx, kMidMs, true);



            Assert::IsTrue (currentPx > 0.0f && currentPx < kDistancePx, L"the first slide is partway");
            Assert::AreEqual (currentPx + kFurtherPx, second.GetOffset (kMidMs), kTolerancePx,
                              L"the second slide starts where the first had reached, plus the new distance");
            Assert::AreEqual (0.0f, second.GetOffset (kMidMs + DxuiPopupMenu::kRevealMs), kTolerancePx);
        }



        TEST_METHOD (ADefaultSlideIsDoneWithNothingToTravel)
        {
            DxuiSlide  slide;



            Assert::IsTrue (slide.IsDone (0));
            Assert::AreEqual (0.0f, slide.GetOffset (0), kTolerancePx);
        }
    };
}
