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
        Assert::AreEqual ((int64_t) 3000, UpdateIndicatorModel::kSweepMs, L"three seconds in all");
        Assert::AreEqual ((int64_t) 8000, UpdateIndicatorModel::kSweepPeriodMs, L"start to start");

        phase = UpdateIndicatorModel::GetSweepPhase (0.0f);
        Assert::AreEqual (0.0f, *phase.lead);
        Assert::IsFalse  (phase.band.has_value(), L"the lead pass comes first, without the band");

        phase = UpdateIndicatorModel::GetSweepPhase (1.0f / 6.0f);
        Assert::AreEqual (0.5f, *phase.lead, 0.0001f);

        phase = UpdateIndicatorModel::GetSweepPhase (1.0f / 3.0f);
        Assert::IsFalse  (phase.lead.has_value(), L"the lead pass has finished");
        Assert::AreEqual (0.0f, *phase.band, 0.0001f, L"before the band starts");

        phase = UpdateIndicatorModel::GetSweepPhase (2.0f / 3.0f);
        Assert::AreEqual (0.5f, *phase.band, 0.0001f);
    }



    TEST_METHOD (Glints_AlternateEdgesAndStayInsideTheText)
    {
        std::vector<IndicatorGlint>  glints = UpdateIndicatorModel::GetGlints (SweepPhase { std::nullopt, 0.5f }, 100.0f, 200.0f, 5.0f, 27.0f);
        size_t                       i      = 0;



        Assert::AreEqual ((size_t) UpdateIndicatorModel::kGlintCount, glints.size());

        for (i = 0; i < glints.size(); i++)
        {
            Assert::IsTrue   (glints[i].x > 100.0f && glints[i].x < 300.0f, L"over the text, never the buttons");
            Assert::AreEqual (i % 2 == 0 ? 5.0f : 27.0f, glints[i].y, L"top, bottom, top, bottom");
            Assert::IsTrue   (glints[i].intensity >= 0.0f && glints[i].intensity <= 1.0f);
        }

        Assert::IsTrue (glints[0].x < glints[1].x && glints[1].x < glints[2].x && glints[2].x < glints[3].x);
    }



    //  Each glint twinkles twice a sweep: once as the lead pass reaches it,
    //  once as the band does, and only then.
    TEST_METHOD (Glints_TwinkleOnceInTheLeadPassAndOnceWithTheBand)
    {
        std::vector<IndicatorGlint>  glints;
        int                          count = UpdateIndicatorModel::kGlintCount;
        int                          i     = 0;
        int                          j     = 0;
        int                          pass  = 0;
        float                        at    = 0.0f;
        SweepPhase                   phase;



        for (pass = 0; pass < 2; pass++)
        {
            for (i = 0; i < count; i++)
            {
                at     = ((float) i + 0.5f) / (float) count;
                phase  = (pass == 0) ? SweepPhase { at, std::nullopt } : SweepPhase { std::nullopt, at };
                glints = UpdateIndicatorModel::GetGlints (phase, 0.0f, 100.0f, 0.0f, 10.0f);

                Assert::AreEqual (1.0f, glints[(size_t) i].intensity, pass == 0 ? L"full as the lead pass reaches it"
                                                                                : L"full as the band reaches it");

                for (j = 0; j < count; j++)
                {
                    if (j != i)
                    {
                        Assert::AreEqual (0.0f, glints[(size_t) j].intensity, L"the others are dark");
                    }
                }
            }
        }

        Assert::AreEqual (0.5f, UpdateIndicatorModel::GetTwinkle (0.125f + UpdateIndicatorModel::kTwinkleSpan * 0.25f, 0.125f),
                          0.001f, L"halfway down from full");
        Assert::AreEqual (0.0f, UpdateIndicatorModel::GetTwinkle (std::nullopt, 0.125f), L"no pass, no twinkle");

        for (const IndicatorGlint & glint : UpdateIndicatorModel::GetGlints (SweepPhase { std::nullopt, 0.0f }, 0.0f, 100.0f, 0.0f, 10.0f))
        {
            Assert::AreEqual (0.0f, glint.intensity, L"dark between the lead pass and the band");
        }
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
