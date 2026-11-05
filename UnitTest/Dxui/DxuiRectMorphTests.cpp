#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiRectMorphTests
//
//  A rectangle moved and resized from one place to another on the ease-out
//  curve, as a pane shrinks into its edge tab when it is pinned. Each morph
//  here is given its time and animation flag as arguments, so no test reads
//  the clock or the system's animation setting.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiRectMorphTests
{
    static constexpr RECT     s_kFrom    = { 100, 200, 500, 600 };
    static constexpr RECT     s_kTo      = { 0, 580, 80, 600 };
    static constexpr int64_t  s_kStartMs = 1000;



    static void AssertRect (const RECT & expected, const RECT & actual, const wchar_t * message)
    {
        Assert::AreEqual (expected.left,   actual.left,   message);
        Assert::AreEqual (expected.top,    actual.top,    message);
        Assert::AreEqual (expected.right,  actual.right,  message);
        Assert::AreEqual (expected.bottom, actual.bottom, message);
    }



    TEST_CLASS (DxuiRectMorphTests)
    {
    public:

        TEST_METHOD (InterpolateRunsEachEdgeFromOneRectToTheOther)
        {
            AssertRect (s_kFrom, DxuiRectMorph::Interpolate (s_kFrom, s_kTo, 0.0f), L"all the way back at 0");
            AssertRect (s_kTo,   DxuiRectMorph::Interpolate (s_kFrom, s_kTo, 1.0f), L"all the way there at 1");
            AssertRect (RECT { 50, 390, 290, 600 }, DxuiRectMorph::Interpolate (s_kFrom, s_kTo, 0.5f), L"halfway at a half");
        }


        TEST_METHOD (TheRectStartsWhereTheMorphStarts)
        {
            DxuiRectMorph  morph = DxuiRectMorph::Start (s_kFrom, s_kTo, s_kStartMs, true);



            AssertRect     (s_kFrom, morph.GetRect (s_kStartMs), L"the first frame is the pane's place");
            Assert::IsFalse (morph.IsDone (s_kStartMs));
        }


        TEST_METHOD (TheMidpointFollowsTheEaseOutCurve)
        {
            constexpr int64_t  kHalfMs  = DxuiRectMorph::kDurationMs / 2;
            constexpr float    kHalfway = 0.5f;

            DxuiRectMorph  morph    = DxuiRectMorph::Start (s_kFrom, s_kTo, s_kStartMs, true);
            RECT           expected = DxuiRectMorph::Interpolate (s_kFrom, s_kTo, DxuiAnimation::ApplyEase (DxuiTweenEase::EaseOut, kHalfway));



            AssertRect (expected, morph.GetRect (s_kStartMs + kHalfMs), L"three quarters of the way at half the time");
            Assert::IsTrue (expected.left < 50, L"past the linear halfway point");
        }


        TEST_METHOD (TheRectIsAtItsEndOnceTheDurationRunsOut)
        {
            DxuiRectMorph  morph = DxuiRectMorph::Start (s_kFrom, s_kTo, s_kStartMs, true);



            AssertRect     (s_kTo, morph.GetRect (s_kStartMs + DxuiRectMorph::kDurationMs), L"at the end");
            Assert::IsTrue (morph.IsDone (s_kStartMs + DxuiRectMorph::kDurationMs));
            AssertRect     (s_kTo, morph.GetRect (s_kStartMs + 10 * DxuiRectMorph::kDurationMs), L"and after it");
            Assert::AreEqual (s_kStartMs + DxuiRectMorph::kDurationMs, morph.GetEndMs());
        }


        //  With animations off the rect is at its end from the first frame.
        TEST_METHOD (AMorphWithAnimationsOffIsDoneAtOnce)
        {
            DxuiRectMorph  morph = DxuiRectMorph::Start (s_kFrom, s_kTo, s_kStartMs, false);



            Assert::IsTrue   (morph.IsDone (s_kStartMs));
            AssertRect       (s_kTo, morph.GetRect (s_kStartMs), L"at the end at once");
            Assert::AreEqual (s_kStartMs, morph.GetEndMs());
        }


        //  An empty rect at either end starts a morph that is already done.
        TEST_METHOD (AMorphToOrFromAnEmptyRectIsDoneAtOnce)
        {
            Assert::IsTrue (DxuiRectMorph::Start (RECT {}, s_kTo,   s_kStartMs, true).IsDone (s_kStartMs));
            Assert::IsTrue (DxuiRectMorph::Start (s_kFrom, RECT {}, s_kStartMs, true).IsDone (s_kStartMs));
            Assert::IsTrue (DxuiRectMorph().IsDone (0), L"nor does a default one");
        }
    };
}
