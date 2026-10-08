#include "Pch.h"
#include "../EhmTestHelper.h"
#include "CassoExplorer/Model/RowGrouping.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  RowGroupingTests
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (RowGroupingTests)
{
public:

    //  Tuesday 29 September 2026, in a locale whose week starts on Sunday.
    static RowGrouping::Today  Tuesday (int firstDayOfWeek = 0)
    {
        RowGrouping::Today  today;

        today.year           = 2026;
        today.month          = 9;
        today.day            = 29;
        today.firstDayOfWeek = firstDayOfWeek;

        return today;
    }

    static std::wstring  DateLabel (int year, int month, int day, const RowGrouping::Today & today)
    {
        return RowGrouping::GetDateGroup (true, year, month, day, today).label;
    }



    TEST_METHOD (Dates_PastBuckets)
    {
        RowGrouping::Today  today = Tuesday();

        Assert::AreEqual (std::wstring (L"Today"),              DateLabel (2026,  9, 29, today));
        Assert::AreEqual (std::wstring (L"Yesterday"),          DateLabel (2026,  9, 28, today));
        Assert::AreEqual (std::wstring (L"Earlier this week"),  DateLabel (2026,  9, 27, today));
        Assert::AreEqual (std::wstring (L"Last week"),          DateLabel (2026,  9, 26, today));
        Assert::AreEqual (std::wstring (L"Last week"),          DateLabel (2026,  9, 20, today));
        Assert::AreEqual (std::wstring (L"Earlier this month"), DateLabel (2026,  9, 19, today));
        Assert::AreEqual (std::wstring (L"Earlier this month"), DateLabel (2026,  9,  1, today));
        Assert::AreEqual (std::wstring (L"Last month"),         DateLabel (2026,  8, 31, today));
        Assert::AreEqual (std::wstring (L"Earlier this year"),  DateLabel (2026,  7,  4, today));
        Assert::AreEqual (std::wstring (L"A long time ago"),    DateLabel (2025, 12, 31, today));
    }



    TEST_METHOD (Dates_FutureBuckets)
    {
        RowGrouping::Today  today = Tuesday();

        Assert::AreEqual (std::wstring (L"Tomorrow"),               DateLabel (2026,  9, 30, today));
        Assert::AreEqual (std::wstring (L"Later this week"),        DateLabel (2026, 10,  3, today));
        Assert::AreEqual (std::wstring (L"Next week"),              DateLabel (2026, 10,  4, today));
        Assert::AreEqual (std::wstring (L"Next week"),              DateLabel (2026, 10, 10, today));
        Assert::AreEqual (std::wstring (L"Later this year"),        DateLabel (2026, 10, 11, today));
        Assert::AreEqual (std::wstring (L"Sometime in the future"), DateLabel (2027,  1,  1, today));
    }



    TEST_METHOD (Dates_WeekFollowsTheLocalesFirstDay)
    {
        //  With weeks from Monday, Sunday the 27th is in last week.
        Assert::AreEqual (std::wstring (L"Last week"), DateLabel (2026, 9, 27, Tuesday (1)));
    }



    TEST_METHOD (Dates_YesterdayWinsOverLastWeek)
    {
        RowGrouping::Today  monday = Tuesday (1);

        monday.day = 28;

        Assert::AreEqual (std::wstring (L"Yesterday"), DateLabel (2026, 9, 27, monday));
    }



    TEST_METHOD (Dates_LastMonthAcrossTheYear)
    {
        RowGrouping::Today  today;

        today.year  = 2026;
        today.month = 1;
        today.day   = 20;

        Assert::AreEqual (std::wstring (L"Last month"),      DateLabel (2025, 12, 15, today));
        Assert::AreEqual (std::wstring (L"A long time ago"), DateLabel (2025, 11, 30, today));
    }



    TEST_METHOD (Dates_NoneIsUnknown)
    {
        Assert::AreEqual (std::wstring (L"Unknown"), RowGrouping::GetDateGroup (false, 0, 0, 0, Tuesday()).label);
    }



    TEST_METHOD (Sizes_UpperBoundsAreInclusive)
    {
        static constexpr uint64_t  kKiB = 1024;
        static constexpr uint64_t  kMiB = 1024 * kKiB;
        static constexpr uint64_t  kGiB = 1024 * kMiB;

        Assert::AreEqual (std::wstring (L"Unspecified"),           RowGrouping::GetSizeGroup (true,  123).label);
        Assert::AreEqual (std::wstring (L"Empty (0 KB)"),          RowGrouping::GetSizeGroup (false, 0).label);
        Assert::AreEqual (std::wstring (L"Tiny (0 - 16 KB)"),      RowGrouping::GetSizeGroup (false, 1).label);
        Assert::AreEqual (std::wstring (L"Tiny (0 - 16 KB)"),      RowGrouping::GetSizeGroup (false, 16 * kKiB).label);
        Assert::AreEqual (std::wstring (L"Small (16 KB - 1 MB)"),  RowGrouping::GetSizeGroup (false, 16 * kKiB + 1).label);
        Assert::AreEqual (std::wstring (L"Small (16 KB - 1 MB)"),  RowGrouping::GetSizeGroup (false, kMiB).label);
        Assert::AreEqual (std::wstring (L"Medium (1 - 128 MB)"),   RowGrouping::GetSizeGroup (false, kMiB + 1).label);
        Assert::AreEqual (std::wstring (L"Large (128 MB - 1 GB)"), RowGrouping::GetSizeGroup (false, kGiB).label);
        Assert::AreEqual (std::wstring (L"Huge (1 - 4 GB)"),       RowGrouping::GetSizeGroup (false, 4 * kGiB).label);
        Assert::AreEqual (std::wstring (L"Gigantic (>4 GB)"),      RowGrouping::GetSizeGroup (false, 4 * kGiB + 1).label);
    }



    TEST_METHOD (Names_ByFirstCharacter)
    {
        Assert::AreEqual (std::wstring (L"0 - 9"), RowGrouping::GetNameGroup (L"7up").label);
        Assert::AreEqual (std::wstring (L"A - H"), RowGrouping::GetNameGroup (L"apple").label);
        Assert::AreEqual (std::wstring (L"I - P"), RowGrouping::GetNameGroup (L"Iris").label);
        Assert::AreEqual (std::wstring (L"Q - Z"), RowGrouping::GetNameGroup (L"zeta").label);
        Assert::AreEqual (std::wstring (L"Other"), RowGrouping::GetNameGroup (L"_hidden").label);
        Assert::AreEqual (std::wstring (L"Other"), RowGrouping::GetNameGroup (L"").label);
    }



    TEST_METHOD (Order_DescendingReversesRanks_TypesByLabel)
    {
        RowGrouping::Group  today     = RowGrouping::GetDateGroup (true, 2026, 9, 29, Tuesday());
        RowGrouping::Group  lastWeek  = RowGrouping::GetDateGroup (true, 2026, 9, 22, Tuesday());
        RowGrouping::Group  dsk       = RowGrouping::GetTypeGroup (L"DSK File");
        RowGrouping::Group  folder    = RowGrouping::GetTypeGroup (L"File folder");

        Assert::IsTrue  (RowGrouping::IsBefore (lastWeek, today, false));
        Assert::IsTrue  (RowGrouping::IsBefore (today, lastWeek, true));
        Assert::IsTrue  (RowGrouping::IsBefore (dsk, folder, false));
        Assert::IsFalse (RowGrouping::IsBefore (dsk, folder, true));
    }



    TEST_METHOD (GetGroup_WallClockDateIsReadWithoutAZone)
    {
        CatalogRow  row;

        //  2026-09-29 23:30 as recorded; read in a zone ahead of UTC it would
        //  land on the 30th.
        row.hasModified         = true;
        row.modifiedIsWallClock = true;
        row.modifiedUnix        = 1790724600;

        Assert::AreEqual (std::wstring (L"Today"), RowGrouping::GetGroup (row, RowGrouping::Field::DateModified, Tuesday()).label);
    }
};
