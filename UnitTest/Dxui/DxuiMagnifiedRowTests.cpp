#include "Pch.h"


using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiMagnifiedRowTests
//
//  A row of items laid out for a pointer, and the strength easing in and out.
//  The times are passed in, so the clock is never read.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiMagnifiedRowTests
{
    static constexpr size_t   kCount     = 9;
    static constexpr float    kWidth     = 16.0f;
    static constexpr float    kGap       = 2.0f;
    static constexpr float    kPitch     = kWidth + kGap;
    static constexpr float    kLeft      = 100.0f;
    static constexpr float    kMaxScale  = 2.0f;
    static constexpr float    kTolerance = 0.001f;
    static constexpr int64_t  kStartMs   = 1000;


    static std::array<DxuiMagnifiedItem, kCount>  MakeRow (float maxScale = kMaxScale)
    {
        std::array<DxuiMagnifiedItem, kCount>  items = {};



        for (size_t i = 0; i < kCount; i++)
        {
            items[i] = { kLeft + (float) i * kPitch, kWidth, maxScale };
        }

        return items;
    }


    static float  GetCenter (const DxuiMagnifiedItem & item)
    {
        return item.left + item.width * 0.5f;
    }


    static DxuiMagnifiedRow::Options  MakeOptions (DxuiMagnifiedRow::Anchor anchor, float reachPitches = 2.0f, DxuiMagnifiedRow::Gaps gaps = DxuiMagnifiedRow::Gaps::Fixed)
    {
        DxuiMagnifiedRow::Options  options;



        options.reach  = kPitch * reachPitches;
        options.anchor = anchor;
        options.gaps   = gaps;

        return options;
    }


    TEST_CLASS (DxuiMagnifiedRowTests)
    {
    public:

        TEST_METHOD (UnderThePointer_AnItemIsAtItsLargestScale)
        {
            Assert::AreEqual (kMaxScale, DxuiMagnifiedRow::GetMagnification (0.0f, kPitch * 2.0f, kMaxScale), kTolerance);
        }

        TEST_METHOD (AtTheReachOrPast_AnItemIsAtRest)
        {
            Assert::AreEqual (1.0f, DxuiMagnifiedRow::GetMagnification (kPitch * 2.0f, kPitch * 2.0f, kMaxScale), kTolerance);
            Assert::AreEqual (1.0f, DxuiMagnifiedRow::GetMagnification (-kPitch * 3.0f, kPitch * 2.0f, kMaxScale), kTolerance);
        }

        TEST_METHOD (EitherSideOfThePointer_AnItemGrowsAlike)
        {
            Assert::AreEqual (DxuiMagnifiedRow::GetMagnification (kPitch, kPitch * 2.0f, kMaxScale),
                              DxuiMagnifiedRow::GetMagnification (-kPitch, kPitch * 2.0f, kMaxScale), kTolerance);
        }

        TEST_METHOD (AtNoStrength_EveryAnchorLaysTheRowAtRest)
        {
            std::array<DxuiMagnifiedItem, kCount>  items = MakeRow();
            std::array<DxuiMagnifiedSpan, kCount>  spans = {};



            for (DxuiMagnifiedRow::Anchor anchor : { DxuiMagnifiedRow::Anchor::Center, DxuiMagnifiedRow::Anchor::Pointer, DxuiMagnifiedRow::Anchor::Fit })
            {
                DxuiMagnifiedRow::Lay (items, GetCenter (items[4]), 0.0f, MakeOptions (anchor), spans);

                for (size_t i = 0; i < kCount; i++)
                {
                    Assert::AreEqual (items[i].left,  spans[i].left,  kTolerance);
                    Assert::AreEqual (items[i].width, spans[i].width, kTolerance);
                    Assert::AreEqual (1.0f,           spans[i].scale, kTolerance);
                }
            }
        }

        TEST_METHOD (ACenteredRow_WithAReachOfTwoPitches_IsTheSameWidthWhereverThePointerIs)
        {
            std::array<DxuiMagnifiedItem, kCount>  items = MakeRow();
            std::array<DxuiMagnifiedSpan, kCount>  spans = {};
            float                                  first = 0.0f;



            for (int step = 0; step <= 10; step++)
            {
                float  pointer = GetCenter (items[3]) + kPitch * 2.0f * (float) step / 10.0f;

                DxuiMagnifiedRow::Lay (items, pointer, 1.0f, MakeOptions (DxuiMagnifiedRow::Anchor::Center), spans);

                if (step == 0)
                {
                    first = spans[0].left;
                }

                Assert::AreEqual (first, spans[0].left, kTolerance);
                Assert::AreEqual (kLeft * 2.0f + kPitch * kCount - kGap - first, spans[kCount - 1].left + spans[kCount - 1].width, kTolerance);
            }
        }

        TEST_METHOD (AFittedRow_IsInTheSpanOfTheRowAtRest)
        {
            std::array<DxuiMagnifiedItem, kCount>  items = MakeRow (4.0f);
            std::array<DxuiMagnifiedSpan, kCount>  spans = {};
            float                                  right = items[kCount - 1].left + kWidth;



            for (float pointer : { kLeft, GetCenter (items[2]), GetCenter (items[6]) + 3.0f, right - 1.0f })
            {
                DxuiMagnifiedRow::Lay (items, pointer, 1.0f, MakeOptions (DxuiMagnifiedRow::Anchor::Fit, 3.5f), spans);

                Assert::AreEqual (kLeft, spans[0].left, kTolerance);
                Assert::AreEqual (right, spans[kCount - 1].left + spans[kCount - 1].width, kTolerance);
            }
        }

        TEST_METHOD (AFittedRow_IsWidestUnderThePointer)
        {
            std::array<DxuiMagnifiedItem, kCount>  items = MakeRow (4.0f);
            std::array<DxuiMagnifiedSpan, kCount>  spans = {};



            DxuiMagnifiedRow::Lay (items, GetCenter (items[4]), 1.0f, MakeOptions (DxuiMagnifiedRow::Anchor::Fit, 3.5f), spans);

            Assert::AreEqual (4.0f, spans[4].scale, kTolerance);
            Assert::IsTrue   (spans[4].width > spans[3].width);
            Assert::IsTrue   (spans[3].width > spans[1].width);
            Assert::IsTrue   (spans[0].width < kWidth);
        }

        TEST_METHOD (ARowAnchoredAtThePointer_HasThePointUnderThePointerUnderIt)
        {
            std::array<DxuiMagnifiedItem, kCount>  items   = MakeRow();
            std::array<DxuiMagnifiedSpan, kCount>  spans   = {};
            float                                  pointer = items[3].left + kWidth * 0.3f;



            DxuiMagnifiedRow::Lay (items, pointer, 1.0f, MakeOptions (DxuiMagnifiedRow::Anchor::Pointer), spans);

            Assert::AreEqual (pointer, spans[3].left + spans[3].width * 0.3f, kTolerance);
        }

        TEST_METHOD (GrowingGaps_GrowWithTheItemsBesideThem)
        {
            std::array<DxuiMagnifiedItem, kCount>  items = MakeRow();
            std::array<DxuiMagnifiedSpan, kCount>  spans = {};



            DxuiMagnifiedRow::Lay (items, GetCenter (items[4]), 1.0f, MakeOptions (DxuiMagnifiedRow::Anchor::Pointer, 2.0f, DxuiMagnifiedRow::Gaps::Grow), spans);

            Assert::AreEqual (kGap * (spans[4].scale + spans[5].scale) * 0.5f, spans[5].left - (spans[4].left + spans[4].width), kTolerance);
            Assert::AreEqual (kGap, spans[1].left - (spans[0].left + spans[0].width), kTolerance);
        }

        TEST_METHOD (TheItemAtAPoint_HasTheGapAfterIt)
        {
            std::array<DxuiMagnifiedItem, kCount>  items = MakeRow();



            Assert::AreEqual (-1, DxuiMagnifiedRow::FindItemAt (items, kLeft - 1.0f));
            Assert::AreEqual (0,  DxuiMagnifiedRow::FindItemAt (items, kLeft));
            Assert::AreEqual (0,  DxuiMagnifiedRow::FindItemAt (items, kLeft + kWidth + kGap * 0.5f));
            Assert::AreEqual (1,  DxuiMagnifiedRow::FindItemAt (items, kLeft + kPitch));
            Assert::AreEqual ((int) kCount - 1, DxuiMagnifiedRow::FindItemAt (items, items[kCount - 1].left + kWidth - 1.0f));
            Assert::AreEqual (-1, DxuiMagnifiedRow::FindItemAt (items, items[kCount - 1].left + kWidth));
        }

        TEST_METHOD (TheStrength_EasesInOverTheFadeTime)
        {
            DxuiMagnifiedRow  row (100);



            Assert::AreEqual (0.0f, row.GetStrength (kStartMs), kTolerance);
            row.SetNear (true, kStartMs);

            Assert::AreEqual (0.5f, row.GetStrength (kStartMs + 50),  kTolerance);
            Assert::AreEqual (1.0f, row.GetStrength (kStartMs + 100), kTolerance);
            Assert::AreEqual (1.0f, row.GetStrength (kStartMs + 500), kTolerance);
        }

        TEST_METHOD (TheStrength_EasesOutFromWhereItWas)
        {
            DxuiMagnifiedRow  row (100);



            row.SetNear (true, kStartMs);
            row.SetNear (false, kStartMs + 40);

            Assert::AreEqual (0.4f, row.GetStrength (kStartMs + 40), kTolerance);
            Assert::AreEqual (0.2f, row.GetStrength (kStartMs + 60), kTolerance);
            Assert::IsTrue   (row.IsActive (kStartMs + 60));
            Assert::IsFalse  (row.IsActive (kStartMs + 80));
        }

        TEST_METHOD (TheStrength_IsEasingOnlyUntilItSettles)
        {
            DxuiMagnifiedRow  row (100);



            Assert::IsFalse (row.IsEasing (kStartMs));
            row.SetNear (true, kStartMs);

            Assert::IsTrue  (row.IsEasing (kStartMs + 50));
            Assert::IsFalse (row.IsEasing (kStartMs + 100));
            Assert::IsTrue  (row.IsActive (kStartMs + 100));
        }
    };
}
