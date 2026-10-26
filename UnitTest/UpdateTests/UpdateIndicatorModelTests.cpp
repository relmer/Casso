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
        int64_t  sweep  = DxuiShimmer::kSweepMs;



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
        DxuiSweepPhase  phase;



        Assert::AreEqual ((int64_t) 1000, DxuiShimmer::kLeadMs);
        Assert::AreEqual ((int64_t) 2000, DxuiShimmer::kBandMs);
        Assert::AreEqual ((int64_t) 500,  DxuiShimmer::kBandStartMs, L"the band starts with the lead pass half way across");
        Assert::AreEqual ((int64_t) 3580, DxuiShimmer::kSweepMs, L"until the last possible twinkle ends");
        Assert::IsTrue   (DxuiShimmer::kSweepMs < UpdateIndicatorModel::kSweepPeriodMs, L"and inside the period");
        Assert::AreEqual ((int64_t) 8000, UpdateIndicatorModel::kSweepPeriodMs, L"start to start");

        phase = DxuiShimmer::GetSweepPhase (0.0f);
        Assert::AreEqual (0.0f, *phase.lead);
        Assert::IsFalse  (phase.band.has_value(), L"the lead pass starts alone");

        phase = DxuiShimmer::GetSweepPhase (750.0f / (float) DxuiShimmer::kSweepMs);
        Assert::AreEqual (0.75f,  *phase.lead, 0.0001f);
        Assert::AreEqual (0.125f, *phase.band, 0.0001f, L"the two overlap");

        phase = DxuiShimmer::GetSweepPhase (1500.0f / (float) DxuiShimmer::kSweepMs);
        Assert::IsFalse  (phase.lead.has_value(), L"the lead pass has finished");
        Assert::AreEqual (0.5f, *phase.band, 0.0001f, L"the band is half way");

        phase = DxuiShimmer::GetSweepPhase (3100.0f / (float) DxuiShimmer::kSweepMs);
        Assert::IsFalse  (phase.lead.has_value() || phase.band.has_value(), L"the tail: only the last twinkles fading");
    }



    static DxuiShimmer::RandomIndexFn MakeSeeded (unsigned seed)
    {
        auto  engine = std::make_shared<std::mt19937> (seed);

        return [engine] (size_t count)
        {
            return (size_t) std::uniform_int_distribution<size_t> (0, count - 1) (*engine);
        };
    }



    TEST_METHOD (Glints_RescatterAtEachSweep)
    {
        UpdateIndicatorButton  indicator;
        int64_t                first     = UpdateIndicatorModel::kFirstSweepMs;
        DxuiSweepGlints        before;



        indicator.SetVisible           (true);
        indicator.SetShowsText         (true);
        indicator.SetText              (L"New toys await");
        indicator.SetAnimationsEnabled (true);
        indicator.SetRandomSource      (MakeSeeded (3));
        indicator.StartShimmerClock    (0);

        indicator.TickShimmer (first + 10);
        before = indicator.GetSweepGlints();

        Assert::IsFalse (before.lead.at == before.band.at, L"each pass scattered on its own");

        indicator.TickShimmer (first + 100);
        Assert::IsTrue (before.lead.at == indicator.GetSweepGlints().lead.at && before.band.at == indicator.GetSweepGlints().band.at,
                        L"fixed within a sweep");

        indicator.TickShimmer (first + DxuiShimmer::kSweepMs + 100);
        indicator.TickShimmer (first + UpdateIndicatorModel::kSweepPeriodMs + 10);
        Assert::IsFalse (before.lead.at == indicator.GetSweepGlints().lead.at, L"scattered afresh for the next");
    }


    //  The tip is placed half the pointer's reach and gap beyond the
    //  indicator: below it, or above it where below would leave the work area.
    TEST_METHOD (Tip_SitsClearOfThePointer)
    {
        constexpr int                kGapPx  = 4;
        RECT                         bounds  = { 900, 0, 1050, 32 };
        DxuiTooltip::PointerExtent   pointer = { 2, 20 };
        RECT                         anchor  = UpdateIndicatorButton::GetTipAnchorPx (bounds, pointer, kGapPx);
        RECT                         work    = { 0, 0, 1920, 1040 };
        RECT                         placed  = {};
        SIZE                         tipPx   = { 200, 22 };



        Assert::AreEqual (bounds.left,                 anchor.left,   L"along the indicator");
        Assert::AreEqual (bounds.bottom + (20 + kGapPx) / 2, anchor.bottom, L"half the pointer's reach below, and half the gap");
        Assert::AreEqual (bounds.top - (2 + kGapPx) / 2,     anchor.top);

        placed = DxuiPopupHost::ComputePlacementForTest (anchor, work, DxuiPopupPlacement::Below, tipPx, true);
        Assert::IsTrue (placed.top >= bounds.bottom + 10, L"half the pointer's reach below the indicator");
    }


    TEST_METHOD (Hover_StartsASweepNowButNeverRestartsOne)
    {
        UpdateIndicatorButton  indicator;
        int64_t                first = UpdateIndicatorModel::kFirstSweepMs;
        int64_t                hover = 1000 + first + DxuiShimmer::kSweepMs + 500;



        indicator.SetVisible           (true);
        indicator.SetShowsText         (true);
        indicator.SetText              (L"New toys await");
        indicator.SetAnimationsEnabled (true);
        indicator.StartShimmerClock    (1000);

        //  Resting between sweeps: entering starts one at once.
        Assert::IsTrue  (indicator.OnPointer (true, hover).showTip);
        Assert::IsTrue  (indicator.TickShimmer (hover), L"sweeping from the moment of the hover");
        Assert::AreEqual ((int64_t) UpdateIndicatorModel::kSweepPeriodMs - DxuiShimmer::kSweepMs,
                          *indicator.GetMsUntilShimmer (hover + DxuiShimmer::kSweepMs),
                          L"the next periodic sweep is a full period after the hover's");

        //  Leaving and coming back mid-sweep does not restart it.
        Assert::IsTrue  (indicator.OnPointer (false, hover + 500).hideTip, L"the tip goes the moment the pointer leaves");
        indicator.OnPointer (true, hover + 700);
        Assert::AreEqual ((int64_t) 0, *indicator.GetMsUntilShimmer (hover + 700));
        Assert::IsFalse (indicator.TickShimmer (hover + DxuiShimmer::kSweepMs + 10) &&
                         indicator.TickShimmer (hover + DxuiShimmer::kSweepMs + 20),
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
        Assert::IsTrue  (indicator.TickShimmer (1000 + first + DxuiShimmer::kSweepMs), L"one more frame to clear it");
        Assert::IsFalse (indicator.TickShimmer (1000 + first + DxuiShimmer::kSweepMs + 50), L"then rest");
        Assert::AreEqual ((int64_t) UpdateIndicatorModel::kSweepPeriodMs - DxuiShimmer::kSweepMs - 50,
                          *indicator.GetMsUntilShimmer (1000 + first + DxuiShimmer::kSweepMs + 50));

        indicator.SetAnimationsEnabled (false);
        Assert::IsFalse (indicator.TickShimmer (1000 + first + 100), L"static text with animations off");
        Assert::IsFalse (indicator.GetMsUntilShimmer (1000).has_value(), L"and no wake-ups");
    }
};
