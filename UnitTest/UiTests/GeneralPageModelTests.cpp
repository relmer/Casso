#include "Pch.h"

#include "Ui/Settings/GeneralPageModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  GeneralPageModelTests
//
//  The General page's text and consent mapping: the last-checked line by
//  how long ago the check ran, the skipped-release line, and the download
//  offers over their consent strings.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (GeneralPageModelTests)
{
public:

    static SYSTEMTIME MakeTime (WORD year, WORD month, WORD day, WORD hour, WORD minute)
    {
        SYSTEMTIME  st = {};

        st.wYear   = year;
        st.wMonth  = month;
        st.wDay    = day;
        st.wHour   = hour;
        st.wMinute = minute;

        return st;
    }


    TEST_METHOD (LastChecked_NeverWhenNoTime)
    {
        SYSTEMTIME  now = MakeTime (2026, 10, 7, 12, 0);

        Assert::AreEqual (L"Never checked.", GeneralPageModel::MakeLastCheckedText (nullptr, now).c_str());
    }


    TEST_METHOD (LastChecked_TodayShowsMorningTime)
    {
        SYSTEMTIME  checked = MakeTime (2026, 10, 7, 8, 46);
        SYSTEMTIME  now     = MakeTime (2026, 10, 7, 23, 59);

        Assert::AreEqual (L"Last checked today at 8:46 AM.", GeneralPageModel::MakeLastCheckedText (&checked, now).c_str());
    }


    TEST_METHOD (LastChecked_MidnightAndNoonUseTwelve)
    {
        SYSTEMTIME  midnight = MakeTime (2026, 10, 7, 0, 5);
        SYSTEMTIME  noon     = MakeTime (2026, 10, 7, 12, 30);
        SYSTEMTIME  now      = MakeTime (2026, 10, 7, 13, 0);

        Assert::AreEqual (L"Last checked today at 12:05 AM.", GeneralPageModel::MakeLastCheckedText (&midnight, now).c_str());
        Assert::AreEqual (L"Last checked today at 12:30 PM.", GeneralPageModel::MakeLastCheckedText (&noon,     now).c_str());
    }


    TEST_METHOD (LastChecked_YesterdayAcrossAYearBoundary)
    {
        SYSTEMTIME  checked = MakeTime (2025, 12, 31, 21, 7);
        SYSTEMTIME  now     = MakeTime (2026, 1, 1, 0, 1);

        Assert::AreEqual (L"Last checked yesterday at 9:07 PM.", GeneralPageModel::MakeLastCheckedText (&checked, now).c_str());
    }


    TEST_METHOD (LastChecked_OlderThisYearShowsMonthAndDay)
    {
        SYSTEMTIME  checked = MakeTime (2026, 3, 1, 10, 0);
        SYSTEMTIME  now     = MakeTime (2026, 3, 3, 10, 0);

        Assert::AreEqual (L"Last checked on Mar 1.", GeneralPageModel::MakeLastCheckedText (&checked, now).c_str());
    }


    TEST_METHOD (LastChecked_EarlierYearAddsTheYear)
    {
        SYSTEMTIME  checked = MakeTime (2025, 11, 20, 10, 0);
        SYSTEMTIME  now     = MakeTime (2026, 10, 7, 10, 0);

        Assert::AreEqual (L"Last checked on Nov 20, 2025.", GeneralPageModel::MakeLastCheckedText (&checked, now).c_str());
    }


    TEST_METHOD (TryGetLocalTime_ZeroIsNever)
    {
        SYSTEMTIME  local = {};

        Assert::IsFalse (GeneralPageModel::TryGetLocalTime (0, local));
    }


    TEST_METHOD (SkippedText_ShowsTheVersion)
    {
        Assert::AreEqual (L"Skipped version: 1.31.91", GeneralPageModel::MakeSkippedText ("1.31.91").c_str());
    }


    TEST_METHOD (Consent_OnlyDeclineUnchecks)
    {
        Assert::IsTrue  (GeneralPageModel::IsOfferChecked ("ask"));
        Assert::IsTrue  (GeneralPageModel::IsOfferChecked ("allow"));
        Assert::IsTrue  (GeneralPageModel::IsOfferChecked (""));
        Assert::IsFalse (GeneralPageModel::IsOfferChecked ("decline"));
    }


    TEST_METHOD (Consent_CheckedAsksUncheckedDeclines)
    {
        Assert::AreEqual (std::string ("ask"),     GeneralPageModel::MakeConsentFromChecked (true));
        Assert::AreEqual (std::string ("decline"), GeneralPageModel::MakeConsentFromChecked (false));
    }
};
