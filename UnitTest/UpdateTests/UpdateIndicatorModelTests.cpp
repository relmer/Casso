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
