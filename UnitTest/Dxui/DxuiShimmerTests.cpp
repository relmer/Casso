#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiShimmerTests
//
//  The shimmer's sweep: its twinkles, where its glints sit and when they
//  light, and the band's falloff.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiShimmerTests)
{
public:

    static DxuiShimmer::RandomIndexFn MakeSeeded (unsigned seed)
    {
        auto  engine = std::make_shared<std::mt19937> (seed);

        return [engine] (size_t count)
        {
            return (size_t) std::uniform_int_distribution<size_t> (0, count - 1) (*engine);
        };
    }



    static void AssertLayoutRules (const DxuiGlintLayout & layout)
    {
        int  i = 0;



        for (i = 0; i < DxuiShimmer::kGlintCount; i++)
        {
            Assert::IsTrue (layout.at[(size_t) i] >= DxuiShimmer::kGlintEdge &&
                            layout.at[(size_t) i] <= 1.0f - DxuiShimmer::kGlintEdge, L"inside the text");

            if (i > 0)
            {
                Assert::IsTrue (layout.at[(size_t) i] - layout.at[(size_t) i - 1] >= DxuiShimmer::kGlintSpacing - 0.0001f,
                                L"sorted, and never crowding");
            }
        }
    }



    //  A twinkle is a fade measured in milliseconds: up over 500 ms, held for
    //  80, down over 500, continuous, 0 at both ends and 1 at the peak.
    TEST_METHOD (Twinkle_FadesInHoldsAndFadesOut)
    {
        float  previous = 0.0f;
        float  value    = 0.0f;
        float  peak     = 0.0f;
        int    ms       = 0;



        Assert::AreEqual ((int64_t) 500, DxuiShimmer::kTwinkleRiseMs);
        Assert::AreEqual ((int64_t) 80,  DxuiShimmer::kTwinkleHoldMs);
        Assert::AreEqual ((int64_t) 500, DxuiShimmer::kTwinkleFallMs);
        Assert::AreEqual (0.0f, DxuiShimmer::GetTwinkle (-5.0f), L"nothing before it starts");
        Assert::AreEqual (0.0f, DxuiShimmer::GetTwinkle (0.0f),  L"starts at 0");
        Assert::AreEqual (0.5f, DxuiShimmer::GetTwinkle (250.0f), 0.0001f, L"half way up");
        Assert::AreEqual (1.0f, DxuiShimmer::GetTwinkle (540.0f), L"held at full");
        Assert::AreEqual (0.0f, DxuiShimmer::GetTwinkle ((float) DxuiShimmer::kTwinkleMs), L"ends at 0");
        Assert::AreEqual (0.0f, DxuiShimmer::GetTwinkle (5000.0f));

        for (ms = 0; ms <= (int) DxuiShimmer::kTwinkleMs; ms++)
        {
            value = DxuiShimmer::GetTwinkle ((float) ms);

            Assert::IsTrue (std::abs (value - previous) < 0.02f, L"continuous at one-millisecond steps: a fade, not a blink");
            peak     = std::max (peak, value);
            previous = value;
        }

        Assert::AreEqual (1.0f, peak);
    }



    TEST_METHOD (Glints_StayInsideTheTextOnTheirEdges)
    {
        DxuiSweepGlints                sweepGlints = DxuiShimmer::MakeEvenSweepGlints();
        std::vector<DxuiShimmerGlint>  glints      = DxuiShimmer::GetGlints (1500.0f, sweepGlints, 100.0f, 200.0f, 5.0f, 27.0f);
        size_t                         i           = 0;
        size_t                         count       = (size_t) DxuiShimmer::kGlintCount;
        bool                           isTop       = false;



        Assert::AreEqual (2 * count, glints.size(), L"one set per pass");

        for (i = 0; i < glints.size(); i++)
        {
            isTop = i < count ? sweepGlints.lead.isTop[i] : sweepGlints.band.isTop[i - count];

            Assert::IsTrue   (glints[i].x > 100.0f && glints[i].x < 300.0f, L"over the text, never the buttons");
            Assert::AreEqual (isTop ? 5.0f : 27.0f, glints[i].y);
            Assert::IsTrue   (glints[i].intensity >= 0.0f && glints[i].intensity <= 1.0f);
        }
    }



    //  The lead glints twinkle as the lead pass reaches each one's x, and the
    //  band glints as the band does; each is its own twinkle, left to right
    //  in both passes.
    TEST_METHOD (Glints_TwinkleInTheLeadPassAndWithTheBandInOrder)
    {
        DxuiSweepGlints                sweepGlints = DxuiShimmer::MakeEvenSweepGlints();
        float                          peakMs      = (float) (DxuiShimmer::kTwinkleRiseMs + DxuiShimmer::kTwinkleHoldMs / 2);
        float                          leadStart   = 0.0f;
        float                          bandStart   = 0.0f;
        float                          lastLead    = -1.0f;
        float                          lastBand    = -1.0f;
        float                          ms          = 0.0f;
        size_t                         i           = 0;
        size_t                         count       = (size_t) DxuiShimmer::kGlintCount;
        std::vector<DxuiShimmerGlint>  glints;
        const float                    kStepMs     = 10.0f;



        sweepGlints.band.at = { 0.10f, 0.35f, 0.60f, 0.85f };

        for (i = 0; i < count; i++)
        {
            leadStart = (float) DxuiShimmer::kLeadMs * sweepGlints.lead.at[i];
            bandStart = (float) DxuiShimmer::kBandStartMs + (float) DxuiShimmer::kBandMs * sweepGlints.band.at[i];

            Assert::AreEqual (leadStart, DxuiShimmer::GetLeadStartMs (sweepGlints.lead.at[i]));
            Assert::AreEqual (bandStart, DxuiShimmer::GetBandStartMs (sweepGlints.band.at[i]));
            Assert::AreEqual (1.0f, DxuiShimmer::GetGlints (leadStart + peakMs, sweepGlints, 0.0f, 100.0f, 0.0f, 10.0f)[i].intensity,
                              L"full in the lead pass");
            Assert::AreEqual (1.0f, DxuiShimmer::GetGlints (bandStart + peakMs, sweepGlints, 0.0f, 100.0f, 0.0f, 10.0f)[count + i].intensity,
                              L"full with the band");
            Assert::AreEqual (sweepGlints.band.at[i] * 100.0f, DxuiShimmer::GetGlints (0.0f, sweepGlints, 0.0f, 100.0f, 0.0f, 10.0f)[count + i].x,
                              0.0001f, L"a band glint sits at its own x");

            for (ms = 0.0f; ms < (float) DxuiShimmer::kSweepMs; ms += kStepMs)
            {
                glints = DxuiShimmer::GetGlints (ms, sweepGlints, 0.0f, 100.0f, 0.0f, 10.0f);

                Assert::AreEqual (DxuiShimmer::GetTwinkle (ms - leadStart), glints[i].intensity,         0.0001f, L"a lead glint has only its own twinkle");
                Assert::AreEqual (DxuiShimmer::GetTwinkle (ms - bandStart), glints[count + i].intensity, 0.0001f, L"a band glint has only its own twinkle");
            }

            Assert::IsTrue   (leadStart > lastLead && bandStart > lastBand, L"left to right in both passes");

            lastLead = leadStart;
            lastBand = bandStart;
        }

        Assert::IsTrue (lastBand + (float) DxuiShimmer::kTwinkleMs <= (float) DxuiShimmer::kSweepMs,
                        L"frames run until the last twinkle has faded");
    }



    TEST_METHOD (GlintLayout_RandomButInRangeSortedAndSpaced)
    {
        DxuiGlintLayout  first;
        DxuiGlintLayout  second;
        unsigned         seed   = 0;
        int              i      = 0;
        bool             isSame = true;



        for (seed = 1; seed <= 50; seed++)
        {
            DxuiGlintLayout  layout = DxuiShimmer::MakeGlintLayout (MakeSeeded (seed));

            for (i = 0; i < DxuiShimmer::kGlintCount; i++)
            {
                Assert::IsTrue (layout.at[(size_t) i] >= DxuiShimmer::kGlintEdge &&
                                layout.at[(size_t) i] <= 1.0f - DxuiShimmer::kGlintEdge, L"inside the text");

                if (i > 0)
                {
                    Assert::IsTrue (layout.at[(size_t) i] - layout.at[(size_t) i - 1] >= DxuiShimmer::kGlintSpacing - 0.0001f,
                                    L"sorted, and never crowding");
                }
            }
        }

        first  = DxuiShimmer::MakeGlintLayout (MakeSeeded (7));
        second = DxuiShimmer::MakeGlintLayout (MakeSeeded (8));

        for (i = 0; i < DxuiShimmer::kGlintCount; i++)
        {
            isSame = isSame && first.at[(size_t) i] == second.at[(size_t) i];
        }

        Assert::IsFalse (isSame, L"two seeds, two layouts");
        Assert::IsTrue  (DxuiShimmer::MakeGlintLayout ([] (size_t) { return (size_t) 0; }).at ==
                         DxuiShimmer::MakeEvenGlintLayout().at, L"a source that keeps crowding falls back to the even spread");
    }



    //  Each pass draws its own layout, both obeying the layout rules, and the
    //  band's is redrawn until no glint of it, lit, runs into a lit lead one.
    TEST_METHOD (SweepGlints_EachPassHasItsOwnLayoutAndNoneClash)
    {
        DxuiSweepGlints  sweepGlints;
        unsigned         seed        = 0;
        int              differ      = 0;
        DxuiSweepGlints  crowded     = DxuiShimmer::MakeSweepGlints ([] (size_t) { return (size_t) 0; });



        for (seed = 1; seed <= 50; seed++)
        {
            sweepGlints = DxuiShimmer::MakeSweepGlints (MakeSeeded (seed));

            AssertLayoutRules (sweepGlints.lead);
            AssertLayoutRules (sweepGlints.band);
            Assert::IsFalse   (DxuiShimmer::DoGlintsClash (sweepGlints), L"no lit glints run into each other");

            differ += sweepGlints.lead.at != sweepGlints.band.at ? 1 : 0;
        }

        Assert::AreEqual (50, differ, L"the two passes land at different spots");
        Assert::IsFalse  (DxuiShimmer::DoGlintsClash (DxuiShimmer::MakeEvenSweepGlints()));
        Assert::IsFalse  (DxuiShimmer::DoGlintsClash (crowded), L"a source that keeps clashing falls back to the opposite edges");
        Assert::IsTrue   (crowded.lead.at == crowded.band.at && crowded.lead.isTop != crowded.band.isTop);
    }



    TEST_METHOD (SweepGlints_ClashOnlyWhenCloseOnOneEdgeAndLitTogether)
    {
        DxuiSweepGlints  sweepGlints = DxuiShimmer::MakeEvenSweepGlints();



        sweepGlints.band = sweepGlints.lead;
        Assert::IsTrue  (DxuiShimmer::DoGlintsClash (sweepGlints), L"the first spot is lit by both passes at once");

        sweepGlints.lead.at    = { 0.80f, 0.84f, 0.88f, 0.92f };
        sweepGlints.lead.isTop = { true, true, true, true };
        sweepGlints.band.at    = { 0.06f, 0.20f, 0.34f, 0.48f };
        sweepGlints.band.isTop = { true, true, true, true };
        Assert::IsFalse (DxuiShimmer::DoGlintsClash (sweepGlints), L"far apart on one edge");

        sweepGlints.band.at    = { 0.82f, 0.86f, 0.90f, 0.94f };
        Assert::IsFalse (DxuiShimmer::DoGlintsClash (sweepGlints), L"close, but the lead ones have faded before the band arrives");
    }



    TEST_METHOD (BandWeight_SmoothFromEdgeToCenter)
    {
        float  previous = -1.0f;
        float  weight   = 0.0f;
        int    step     = 0;



        Assert::AreEqual (1.0f, DxuiShimmer::GetBandWeight (0.0f,   10.0f), L"white at the center");
        Assert::AreEqual (0.0f, DxuiShimmer::GetBandWeight (10.0f,  10.0f), L"accent at the edge");
        Assert::AreEqual (0.0f, DxuiShimmer::GetBandWeight (-25.0f, 10.0f), L"and beyond it");
        Assert::AreEqual (0.5f, DxuiShimmer::GetBandWeight (5.0f,   10.0f), 0.0001f, L"half way between");
        Assert::AreEqual (DxuiShimmer::GetBandWeight (-3.0f, 10.0f), DxuiShimmer::GetBandWeight (3.0f, 10.0f),
                          L"symmetric");
        Assert::AreEqual (0.0f, DxuiShimmer::GetBandWeight (0.0f, 0.0f), L"no band, no shimmer");

        //  Rising from the edge to the center with no jump: monotonic, and no
        //  step between neighboring samples larger than a smooth curve allows.
        for (step = 100; step >= 0; step--)
        {
            weight = DxuiShimmer::GetBandWeight ((float) step * 0.1f, 10.0f);

            Assert::IsTrue (weight >= previous, L"monotonic toward the center");
            Assert::IsTrue (previous < 0.0f || weight - previous < 0.02f, L"continuous");
            previous = weight;
        }

        Assert::IsTrue (DxuiShimmer::GetBandWeight (9.9f, 10.0f) < 0.001f, L"soft at the edge, no hard cutover");
    }

};