#include "Pch.h"

#include "Core/UnicodeSymbols.h"
#include "Ui/Chrome/UpdateIndicatorButton.h"
#include "Ui/Chrome/UpdateIndicatorModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  UpdateIndicatorModelTests
//
//  The title-bar indicator's line, its fit beside the caption buttons, and
//  its shimmer timing. The animation setting is pinned in every test that
//  depends on it: the CI runner reports animations off.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (UpdateIndicatorModelTests)
{
public:

    static size_t CountWords (const std::wstring & text)
    {
        size_t  words  = 0;
        bool    inWord = false;

        for (wchar_t ch : text)
        {
            bool  isBreak = ch == L' ' || ch == s_kchEmDash;

            words  += (!isBreak && !inWord) ? 1 : 0;
            inWord  = !isBreak;
        }

        return words;
    }



    TEST_METHOD (Lines_AreShortFilledAndSomeCarryTheVersion)
    {
        std::vector<std::wstring>  lines       = UpdateIndicatorModel::GetLines ("1.30.0");
        size_t                     withVersion = 0;



        Assert::IsTrue (lines.size() >= 8);

        for (const std::wstring & line : lines)
        {
            Assert::IsTrue (CountWords (line) >= 2 && CountWords (line) <= 5, line.c_str());
            Assert::IsTrue (line.find_first_of (L"{}") == std::wstring::npos, line.c_str());
            Assert::IsTrue (iswupper (line.front()) || iswdigit (line.front()), line.c_str());
            Assert::IsTrue (line.find (std::wstring (L" ") + s_kchEmDash) == std::wstring::npos &&
                            line.find (std::wstring (1, s_kchEmDash) + L" ") == std::wstring::npos, L"em dashes abut");
            Assert::IsTrue (UpdateIndicatorModel::EstimateTextWidthDip (line) <= 180, L"short enough for a caption");

            withVersion += line.find (L"1.30.0") != std::wstring::npos ? 1 : 0;
        }

        Assert::IsTrue (withVersion >= 2, L"some lines give the version");
        Assert::AreEqual (lines[3], UpdateIndicatorModel::PickLine ("1.30.0", [] (size_t) { return (size_t) 3; }));
    }



    TEST_METHOD (Fit_ShowsTextWhenThereIsRoom)
    {
        IndicatorFit  fit = UpdateIndicatorModel::Fit (1200, 194, L"New toys await");



        Assert::IsTrue   (fit.showsText);
        Assert::AreEqual (UpdateIndicatorModel::kGlyphColumnDip + UpdateIndicatorModel::EstimateTextWidthDip (L"New toys await") +
                          UpdateIndicatorModel::kTextPadDip, fit.widthDip);
    }



    TEST_METHOD (Fit_FallsBackToTheArrowBeforeTheTitleGoes)
    {
        std::wstring  text     = L"New toys await";
        int           withText = UpdateIndicatorModel::kGlyphColumnDip + UpdateIndicatorModel::EstimateTextWidthDip (text) +
                                 UpdateIndicatorModel::kTextPadDip;
        int           exact    = 194 + UpdateIndicatorModel::kMinTitleDip + withText;
        IndicatorFit  fit;



        fit = UpdateIndicatorModel::Fit (exact, 194, text);
        Assert::IsTrue (fit.showsText, L"exactly enough room");

        fit = UpdateIndicatorModel::Fit (exact - 1, 194, text);
        Assert::IsFalse  (fit.showsText, L"one DIP short: the title keeps its minimum and the text goes");
        Assert::AreEqual (UpdateIndicatorModel::kArrowOnlyDip, fit.widthDip);

        Assert::IsFalse (UpdateIndicatorModel::Fit (1200, 194, L"").showsText);
    }



    TEST_METHOD (Sweep_TimingFromElapsedTime)
    {
        int64_t  first  = UpdateIndicatorModel::kFirstSweepMs;
        int64_t  period = UpdateIndicatorModel::kSweepPeriodMs;
        int64_t  sweep  = UpdateIndicatorModel::kSweepMs;



        Assert::IsFalse  (UpdateIndicatorModel::GetSweepProgress (0).has_value(), L"none right after showing");
        Assert::IsFalse  (UpdateIndicatorModel::GetSweepProgress (first - 1).has_value());
        Assert::AreEqual (0.0f, *UpdateIndicatorModel::GetSweepProgress (first));
        Assert::AreEqual (0.5f, *UpdateIndicatorModel::GetSweepProgress (first + sweep / 2));
        Assert::IsFalse  (UpdateIndicatorModel::GetSweepProgress (first + sweep).has_value(), L"one second, then rest");
        Assert::IsFalse  (UpdateIndicatorModel::GetSweepProgress (first + period - 1).has_value());
        Assert::AreEqual (0.25f, *UpdateIndicatorModel::GetSweepProgress (first + period + sweep / 4), L"and again every period");

        Assert::AreEqual (first,              UpdateIndicatorModel::GetMsUntilSweep (0));
        Assert::AreEqual ((int64_t) 0,        UpdateIndicatorModel::GetMsUntilSweep (first + 10), L"due now during a sweep");
        Assert::AreEqual (period - sweep,     UpdateIndicatorModel::GetMsUntilSweep (first + sweep));
    }



    TEST_METHOD (Sweep_IsALeadPassThenTheBandEveryEightSeconds)
    {
        SweepPhase  phase;



        Assert::AreEqual ((int64_t) 1000, UpdateIndicatorModel::kLeadMs);
        Assert::AreEqual ((int64_t) 2000, UpdateIndicatorModel::kBandMs);
        Assert::AreEqual ((int64_t) 500,  UpdateIndicatorModel::kBandStartMs, L"the band starts with the lead pass half way across");
        Assert::AreEqual ((int64_t) 3580, UpdateIndicatorModel::kSweepMs, L"until the last possible twinkle ends");
        Assert::IsTrue   (UpdateIndicatorModel::kSweepMs < UpdateIndicatorModel::kSweepPeriodMs, L"and inside the period");
        Assert::AreEqual ((int64_t) 8000, UpdateIndicatorModel::kSweepPeriodMs, L"start to start");

        phase = UpdateIndicatorModel::GetSweepPhase (0.0f);
        Assert::AreEqual (0.0f, *phase.lead);
        Assert::IsFalse  (phase.band.has_value(), L"the lead pass starts alone");

        phase = UpdateIndicatorModel::GetSweepPhase (750.0f / (float) UpdateIndicatorModel::kSweepMs);
        Assert::AreEqual (0.75f,  *phase.lead, 0.0001f);
        Assert::AreEqual (0.125f, *phase.band, 0.0001f, L"the two overlap");

        phase = UpdateIndicatorModel::GetSweepPhase (1500.0f / (float) UpdateIndicatorModel::kSweepMs);
        Assert::IsFalse  (phase.lead.has_value(), L"the lead pass has finished");
        Assert::AreEqual (0.5f, *phase.band, 0.0001f, L"the band is half way");

        phase = UpdateIndicatorModel::GetSweepPhase (3100.0f / (float) UpdateIndicatorModel::kSweepMs);
        Assert::IsFalse  (phase.lead.has_value() || phase.band.has_value(), L"the tail: only the last twinkles fading");
    }



    //  A twinkle is a fade measured in milliseconds: up over 500 ms, held for
    //  80, down over 500, continuous, 0 at both ends and 1 at the peak.
    TEST_METHOD (Twinkle_FadesInHoldsAndFadesOut)
    {
        float  previous = 0.0f;
        float  value    = 0.0f;
        float  peak     = 0.0f;
        int    ms       = 0;



        Assert::AreEqual ((int64_t) 500, UpdateIndicatorModel::kTwinkleRiseMs);
        Assert::AreEqual ((int64_t) 80,  UpdateIndicatorModel::kTwinkleHoldMs);
        Assert::AreEqual ((int64_t) 500, UpdateIndicatorModel::kTwinkleFallMs);
        Assert::AreEqual (0.0f, UpdateIndicatorModel::GetTwinkle (-5.0f), L"nothing before it starts");
        Assert::AreEqual (0.0f, UpdateIndicatorModel::GetTwinkle (0.0f),  L"starts at 0");
        Assert::AreEqual (0.5f, UpdateIndicatorModel::GetTwinkle (250.0f), 0.0001f, L"half way up");
        Assert::AreEqual (1.0f, UpdateIndicatorModel::GetTwinkle (540.0f), L"held at full");
        Assert::AreEqual (0.0f, UpdateIndicatorModel::GetTwinkle ((float) UpdateIndicatorModel::kTwinkleMs), L"ends at 0");
        Assert::AreEqual (0.0f, UpdateIndicatorModel::GetTwinkle (5000.0f));

        for (ms = 0; ms <= (int) UpdateIndicatorModel::kTwinkleMs; ms++)
        {
            value = UpdateIndicatorModel::GetTwinkle ((float) ms);

            Assert::IsTrue (std::abs (value - previous) < 0.02f, L"continuous at one-millisecond steps: a fade, not a blink");
            peak     = std::max (peak, value);
            previous = value;
        }

        Assert::AreEqual (1.0f, peak);
    }



    TEST_METHOD (Glints_StayInsideTheTextOnTheirEdges)
    {
        GlintLayout                  layout = UpdateIndicatorModel::MakeEvenGlintLayout();
        std::vector<IndicatorGlint>  glints = UpdateIndicatorModel::GetGlints (1500.0f, layout, 100.0f, 200.0f, 5.0f, 27.0f);
        size_t                       i      = 0;



        Assert::AreEqual ((size_t) UpdateIndicatorModel::kGlintCount, glints.size());

        for (i = 0; i < glints.size(); i++)
        {
            Assert::IsTrue   (glints[i].x > 100.0f && glints[i].x < 300.0f, L"over the text, never the buttons");
            Assert::AreEqual (layout.isTop[i] ? 5.0f : 27.0f, glints[i].y);
            Assert::IsTrue   (glints[i].intensity >= 0.0f && glints[i].intensity <= 1.0f);
        }
    }



    //  Each glint twinkles twice a sweep: from the moment the lead pass
    //  reaches its x, and again from the moment the band does; left to
    //  right in both, so the order follows x.
    TEST_METHOD (Glints_TwinkleInTheLeadPassAndWithTheBandInOrder)
    {
        GlintLayout  layout     = UpdateIndicatorModel::MakeEvenGlintLayout();
        float        peakMs     = (float) (UpdateIndicatorModel::kTwinkleRiseMs + UpdateIndicatorModel::kTwinkleHoldMs / 2);
        float        leadStart  = 0.0f;
        float        bandStart  = 0.0f;
        float        lastLead   = -1.0f;
        float        lastBand   = -1.0f;
        float        ms         = 0.0f;
        int          i          = 0;
        const float  kStepMs    = 10.0f;



        for (i = 0; i < UpdateIndicatorModel::kGlintCount; i++)
        {
            leadStart = layout.at[(size_t) i] * (float) UpdateIndicatorModel::kLeadMs;
            bandStart = (float) UpdateIndicatorModel::kBandStartMs + layout.at[(size_t) i] * (float) UpdateIndicatorModel::kBandMs;

            Assert::AreEqual (1.0f, UpdateIndicatorModel::GetGlints (leadStart + peakMs, layout, 0.0f, 100.0f, 0.0f, 10.0f)[(size_t) i].intensity,
                              L"full in the lead pass");
            Assert::AreEqual (1.0f, UpdateIndicatorModel::GetGlints (bandStart + peakMs, layout, 0.0f, 100.0f, 0.0f, 10.0f)[(size_t) i].intensity,
                              L"full again with the band");

            for (ms = leadStart; ms < bandStart + (float) UpdateIndicatorModel::kTwinkleMs; ms += kStepMs)
            {
                Assert::AreEqual (std::max (UpdateIndicatorModel::GetTwinkle (ms - leadStart), UpdateIndicatorModel::GetTwinkle (ms - bandStart)),
                                  UpdateIndicatorModel::GetGlints (ms, layout, 0.0f, 100.0f, 0.0f, 10.0f)[(size_t) i].intensity,
                                  0.0001f, L"overlapping twinkles combine by max, never summing");
            }

            Assert::IsTrue   (leadStart > lastLead && bandStart > lastBand, L"left to right in both passes");

            lastLead = leadStart;
            lastBand = bandStart;
        }

        Assert::IsTrue (lastBand + (float) UpdateIndicatorModel::kTwinkleMs <= (float) UpdateIndicatorModel::kSweepMs,
                        L"frames run until the last twinkle has faded");
    }



    static UpdateIndicatorModel::RandomIndexFn MakeSeeded (unsigned seed)
    {
        auto  engine = std::make_shared<std::mt19937> (seed);

        return [engine] (size_t count)
        {
            return (size_t) std::uniform_int_distribution<size_t> (0, count - 1) (*engine);
        };
    }



    TEST_METHOD (GlintLayout_RandomButInRangeSortedAndSpaced)
    {
        GlintLayout  first;
        GlintLayout  second;
        unsigned     seed   = 0;
        int          i      = 0;
        bool         isSame = true;



        for (seed = 1; seed <= 50; seed++)
        {
            GlintLayout  layout = UpdateIndicatorModel::MakeGlintLayout (MakeSeeded (seed));

            for (i = 0; i < UpdateIndicatorModel::kGlintCount; i++)
            {
                Assert::IsTrue (layout.at[(size_t) i] >= UpdateIndicatorModel::kGlintEdge &&
                                layout.at[(size_t) i] <= 1.0f - UpdateIndicatorModel::kGlintEdge, L"inside the text");

                if (i > 0)
                {
                    Assert::IsTrue (layout.at[(size_t) i] - layout.at[(size_t) i - 1] >= UpdateIndicatorModel::kGlintSpacing - 0.0001f,
                                    L"sorted, and never crowding");
                }
            }
        }

        first  = UpdateIndicatorModel::MakeGlintLayout (MakeSeeded (7));
        second = UpdateIndicatorModel::MakeGlintLayout (MakeSeeded (8));

        for (i = 0; i < UpdateIndicatorModel::kGlintCount; i++)
        {
            isSame = isSame && first.at[(size_t) i] == second.at[(size_t) i];
        }

        Assert::IsFalse (isSame, L"two seeds, two layouts");
        Assert::IsTrue  (UpdateIndicatorModel::MakeGlintLayout ([] (size_t) { return (size_t) 0; }).at ==
                         UpdateIndicatorModel::MakeEvenGlintLayout().at, L"a source that keeps crowding falls back to the even spread");
    }



    TEST_METHOD (Glints_RescatterAtEachSweep)
    {
        UpdateIndicatorButton  indicator;
        int64_t                first  = UpdateIndicatorModel::kFirstSweepMs;
        GlintLayout            before;



        indicator.SetVisible           (true);
        indicator.SetShowsText         (true);
        indicator.SetText              (L"New toys await");
        indicator.SetAnimationsEnabled (true);
        indicator.SetRandomSource      (MakeSeeded (3));
        indicator.StartShimmerClock    (0);

        indicator.TickShimmer (first + 10);
        before = indicator.GetGlintLayout();

        indicator.TickShimmer (first + 100);
        Assert::IsTrue (before.at == indicator.GetGlintLayout().at, L"fixed within a sweep");

        indicator.TickShimmer (first + UpdateIndicatorModel::kSweepMs + 100);
        indicator.TickShimmer (first + UpdateIndicatorModel::kSweepPeriodMs + 10);
        Assert::IsFalse (before.at == indicator.GetGlintLayout().at, L"scattered afresh for the next");
    }



    TEST_METHOD (Hover_StartsASweepNowButNeverRestartsOne)
    {
        UpdateIndicatorButton  indicator;
        int64_t                first = UpdateIndicatorModel::kFirstSweepMs;
        int64_t                hover = 1000 + first + UpdateIndicatorModel::kSweepMs + 500;



        indicator.SetVisible           (true);
        indicator.SetShowsText         (true);
        indicator.SetText              (L"New toys await");
        indicator.SetAnimationsEnabled (true);
        indicator.StartShimmerClock    (1000);

        //  Resting between sweeps: entering starts one at once.
        Assert::IsTrue  (indicator.OnPointer (true, hover).showTip);
        Assert::IsTrue  (indicator.TickShimmer (hover), L"sweeping from the moment of the hover");
        Assert::AreEqual ((int64_t) UpdateIndicatorModel::kSweepPeriodMs - UpdateIndicatorModel::kSweepMs,
                          *indicator.GetMsUntilShimmer (hover + UpdateIndicatorModel::kSweepMs),
                          L"the next periodic sweep is a full period after the hover's");

        //  Leaving and coming back mid-sweep does not restart it.
        Assert::IsTrue  (indicator.OnPointer (false, hover + 500).hideTip, L"the tip goes the moment the pointer leaves");
        indicator.OnPointer (true, hover + 700);
        Assert::AreEqual ((int64_t) 0, *indicator.GetMsUntilShimmer (hover + 700));
        Assert::IsFalse (indicator.TickShimmer (hover + UpdateIndicatorModel::kSweepMs + 10) &&
                         indicator.TickShimmer (hover + UpdateIndicatorModel::kSweepMs + 20),
                         L"the original sweep ended on time, so it was not restarted at 700 ms");
    }



    TEST_METHOD (Hover_NoSweepWithAnimationsOff)
    {
        UpdateIndicatorButton  indicator;



        indicator.SetVisible           (true);
        indicator.SetShowsText         (true);
        indicator.SetText              (L"New toys await");
        indicator.SetAnimationsEnabled (false);
        indicator.StartShimmerClock    (0);

        Assert::IsTrue  (indicator.OnPointer (true, 100000).showTip, L"the tooltip still shows");
        Assert::IsFalse (indicator.TickShimmer (100000), L"but nothing sweeps");
    }



    TEST_METHOD (BandWeight_SmoothFromEdgeToCenter)
    {
        float  previous = -1.0f;
        float  weight   = 0.0f;
        int    step     = 0;



        Assert::AreEqual (1.0f, UpdateIndicatorModel::GetBandWeight (0.0f,   10.0f), L"white at the center");
        Assert::AreEqual (0.0f, UpdateIndicatorModel::GetBandWeight (10.0f,  10.0f), L"accent at the edge");
        Assert::AreEqual (0.0f, UpdateIndicatorModel::GetBandWeight (-25.0f, 10.0f), L"and beyond it");
        Assert::AreEqual (0.5f, UpdateIndicatorModel::GetBandWeight (5.0f,   10.0f), 0.0001f, L"half way between");
        Assert::AreEqual (UpdateIndicatorModel::GetBandWeight (-3.0f, 10.0f), UpdateIndicatorModel::GetBandWeight (3.0f, 10.0f),
                          L"symmetric");
        Assert::AreEqual (0.0f, UpdateIndicatorModel::GetBandWeight (0.0f, 0.0f), L"no band, no shimmer");

        //  Rising from the edge to the center with no jump: monotonic, and no
        //  step between neighboring samples larger than a smooth curve allows.
        for (step = 100; step >= 0; step--)
        {
            weight = UpdateIndicatorModel::GetBandWeight ((float) step * 0.1f, 10.0f);

            Assert::IsTrue (weight >= previous, L"monotonic toward the center");
            Assert::IsTrue (previous < 0.0f || weight - previous < 0.02f, L"continuous");
            previous = weight;
        }

        Assert::IsTrue (UpdateIndicatorModel::GetBandWeight (9.9f, 10.0f) < 0.001f, L"soft at the edge, no hard cutover");
    }


    TEST_METHOD (Shimmer_FramesOnlyDuringASweepAndNeverWithAnimationsOff)
    {
        UpdateIndicatorButton  indicator;
        int64_t                first = UpdateIndicatorModel::kFirstSweepMs;



        indicator.SetVisible           (true);
        indicator.SetShowsText         (true);
        indicator.SetText              (L"New toys await");
        indicator.SetAnimationsEnabled (true);
        indicator.StartShimmerClock    (1000);

        Assert::IsFalse (indicator.TickShimmer (1000 + first - 1), L"no frames before the sweep");
        Assert::IsTrue  (indicator.TickShimmer (1000 + first + 100));
        Assert::IsTrue  (indicator.TickShimmer (1000 + first + UpdateIndicatorModel::kSweepMs), L"one more frame to clear it");
        Assert::IsFalse (indicator.TickShimmer (1000 + first + UpdateIndicatorModel::kSweepMs + 50), L"then rest");
        Assert::AreEqual ((int64_t) UpdateIndicatorModel::kSweepPeriodMs - UpdateIndicatorModel::kSweepMs - 50,
                          *indicator.GetMsUntilShimmer (1000 + first + UpdateIndicatorModel::kSweepMs + 50));

        indicator.SetAnimationsEnabled (false);
        Assert::IsFalse (indicator.TickShimmer (1000 + first + 100), L"static text with animations off");
        Assert::IsFalse (indicator.GetMsUntilShimmer (1000).has_value(), L"and no wake-ups");
    }
};
