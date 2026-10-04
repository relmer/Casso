#include "Pch.h"

#include "Config/GlobalUserPrefs.h"
#include "Ui/Debugger/ReverseOptionsDialog.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ReverseOptionsDialogTests
//
//  The options dialog's reading of its boxes and its estimate of the history
//  the memory budget holds.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ReverseOptionsDialogTests)
{
public:

    TEST_METHOD (TheBoxesAreReadWithinTheSavedRanges)
    {
        std::optional<ReverseOptions>  parsed = ReverseOptionsDialog::TryParse (false, L" 128 ");



        Assert::IsTrue (parsed.has_value(), L"in range, spaces allowed");
        Assert::IsFalse (parsed->isRecording);
        Assert::AreEqual (128, parsed->budgetMb);

        Assert::IsFalse (ReverseOptionsDialog::TryParse (true, L"3").has_value(), L"below the smallest budget");
        Assert::IsFalse (ReverseOptionsDialog::TryParse (true, L"4097").has_value(), L"above the largest budget");
        Assert::IsFalse (ReverseOptionsDialog::TryParse (true, L"64MB").has_value(), L"not a whole number");
        Assert::IsFalse (ReverseOptionsDialog::TryParse (true, L"6 4").has_value(), L"a space inside the number");
        Assert::IsFalse (ReverseOptionsDialog::TryParse (true, L"").has_value(), L"empty");
        Assert::IsFalse (ReverseOptionsDialog::TryParse (true, L"99999999999").has_value(), L"too long to be a budget");

        Assert::IsTrue (ReverseOptionsDialog::TryParse (true, std::to_wstring (GlobalUserPrefs::kMinReverseBudgetMb)).has_value(), L"the ends of the ranges");
        Assert::IsTrue (ReverseOptionsDialog::TryParse (true, std::to_wstring (GlobalUserPrefs::kMaxReverseBudgetMb)).has_value());
    }


    TEST_METHOD (TheEstimateFollowsTheMeasuredRecordings)
    {
        double  low  = 0.0;
        double  high = 0.0;



        ReverseOptionsDialog::EstimateMinutes (64, 10, low, high);
        Assert::AreEqual (24.1, low,  0.001, L"a game filled 64 MB in 24.1 minutes");
        Assert::AreEqual (43.8, high, 0.001, L"hi-res drawing in 43.8");

        ReverseOptionsDialog::EstimateMinutes (128, 20, low, high);
        Assert::AreEqual (24.1 * 4, low, 0.001, L"twice the memory at half the snapshots holds four times as long");

        Assert::AreEqual (std::wstring (L"Holds about 24 to 44 minutes of history; a busy program fills it sooner."),
                          ReverseOptionsDialog::GetEstimateText (L"64"));
        Assert::IsTrue (ReverseOptionsDialog::GetEstimateText (L"2").starts_with (L"The memory is 4 to 4096 MB."),
                        ReverseOptionsDialog::GetEstimateText (L"2").c_str());
    }
};