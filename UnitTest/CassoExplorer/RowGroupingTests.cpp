#include "Pch.h"
#include "../EhmTestHelper.h"
#include "Core/UnicodeSymbols.h"
#include "CassoExplorer/CassoExplorerShellListings.h"
#include "CassoExplorer/Model/GroupChoices.h"
#include "CassoExplorer/Model/RowGrouping.h"
#include "Seams/Win32PropertyColumns.h"

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

    TEST_METHOD (GroupChoices_NamePropertiesTheShellHas)
    {
        using Folder = GroupChoices::Folder;

        for (Folder folder : { Folder::Generic, Folder::Documents, Folder::OneDrive, Folder::Search, Folder::Drives, Folder::Pictures, Folder::Music, Folder::Videos, Folder::RecycleBin, Folder::Home, Folder::Gallery, Folder::Network, Folder::Zip })
        {
            for (const GroupChoices::Choice & choice : GroupChoices::GetChoices (folder))
            {
                PROPERTYKEY  key = {};
                HRESULT      hr  = (choice.property != nullptr) ? Win32PropertyColumns::GetKey (choice.property, key) : S_OK;

                Assert::IsTrue (SUCCEEDED (hr), choice.label);
            }
        }
    }

    TEST_METHOD (PropertyGroup_ByTextDateOrUnspecified)
    {
        CatalogModel::PropertyKey  key;
        RowGrouping::Today         today = Tuesday();

        Assert::AreEqual (std::wstring (L"Unspecified"), RowGrouping::GetPropertyGroup (key, today).label);

        key.has  = true;
        key.text = L"Rob";
        Assert::AreEqual (std::wstring (L"Rob"), RowGrouping::GetPropertyGroup (key, today).label);

        key.isDate = true;
        key.number = 116444736000000000.0;    // 1970, as a FILETIME
        Assert::AreEqual (std::wstring (L"A long time ago"), RowGrouping::GetPropertyGroup (key, today).label);
    }

    //  A slow read shows what it has so far, and a second read keeps showing
    //  the first one's items until it finishes.
    TEST_METHOD (ShellListing_PublishesProgressAndKeepsItemsDuringTheNextRead)
    {
        using Listings = CassoExplorerListings<int>;

        static constexpr int  s_kTries = 2000;

        Listings               listings;
        HWND                   target  = CreateWindowExW (0, L"STATIC", nullptr, 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, nullptr, nullptr);
        std::vector<int>                 items;
        HRESULT                          result   = S_OK;
        std::atomic<bool>                release  = false;
        thread_local Listings::Progress  progress;

        listings.SetTarget (target, WM_NULL);
        listings.SetProgressSetter ([] (Listings::Progress given) { progress = std::move (given); });
        listings.SetLister ([&release] (const std::wstring &, std::vector<int> & outItems)
        {
            outItems = { 1 };
            progress (outItems);

            while (!release)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
            }

            outItems = { 1, 2 };

            return S_OK;
        });

        Assert::IsFalse (listings.TryTake (L"k", L"id", items, result));

        for (int at = 0; at < s_kTries && !listings.TryGetLatest (L"k", items); at++)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        Assert::AreEqual ((size_t) 1, items.size(), L"The read in progress shows what it has");
        Assert::IsFalse  (listings.TryTake (L"k", L"id", items, result), L"and is not finished");

        release = true;

        for (int at = 0; at < s_kTries && !listings.TryTake (L"k", L"id", items, result); at++)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        Assert::AreEqual ((size_t) 2, items.size());
        Assert::IsFalse  (listings.IsReadOrReading (L"k"), L"A progress message arriving now has nothing to act on");

        //  A second read, still under way, has the first's items to show.
        release = false;
        Assert::IsFalse  (listings.TryTake (L"k", L"id", items, result));
        Assert::IsTrue   (listings.TryGetLatest (L"k", items));
        Assert::IsFalse  (items.empty());

        release = true;

        for (int at = 0; at < s_kTries && !listings.TryTake (L"k", L"id", items, result); at++)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));
        }

        DestroyWindow (target);
    }

    TEST_METHOD (DriveRanges_AreWindowsLabels)
    {
        static constexpr uint64_t  kGiB = 1024ull * 1024ull * 1024ull;

        Assert::AreEqual (std::wstring (L"0 - 16 GB"),       RowGrouping::GetCapacityGroup (16 * kGiB).label);
        Assert::AreEqual (std::wstring (L"16 GB - 80 GB"),   RowGrouping::GetCapacityGroup (16 * kGiB + 1).label);
        Assert::AreEqual (std::wstring (L"250 GB - 500 GB"), RowGrouping::GetCapacityGroup (476 * kGiB).label);
        Assert::AreEqual (std::wstring (L"500 GB - 1 TB"),   RowGrouping::GetCapacityGroup (1024 * kGiB).label);
        Assert::AreEqual (std::wstring (L">1 TB"),           RowGrouping::GetCapacityGroup (1025 * kGiB).label);

        Assert::AreEqual (std::wstring (L"30% - 39%"),       RowGrouping::GetPercentGroup (35.0, L"").label);
        Assert::AreEqual (std::wstring (L"60% - 69% free"),  RowGrouping::GetPercentGroup (65.0, L" free").label);
        Assert::AreEqual (std::wstring (L"100%"),            RowGrouping::GetPercentGroup (100.0, L"").label);
    }

    TEST_METHOD (PropertyGroups_ListAValueOfSeveralPartsInEach)
    {
        CatalogModel::PropertyKey  key;

        key.has   = true;
        key.text  = L"Rob; Sam";
        key.parts = { L"Rob", L"Sam" };

        std::vector<RowGrouping::Group>  groups = RowGrouping::GetPropertyGroups (key, Tuesday());

        Assert::AreEqual ((size_t) 2, groups.size());
        Assert::AreEqual (std::wstring (L"Sam"), groups[1].label);

        key.parts.clear();
        key.groupLabel = L"250 GB - 500 GB";
        Assert::AreEqual (std::wstring (L"250 GB - 500 GB"), RowGrouping::GetPropertyGroup (key, Tuesday()).label);
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

        Assert::AreEqual (std::wstring (L"Unspecified"),                               RowGrouping::GetSizeGroup (true,  123).label);
        Assert::AreEqual (std::wstring (L"Empty (0 KB)"),                              RowGrouping::GetSizeGroup (false, 0).label);
        Assert::AreEqual (std::wstring (L"Tiny (0 ") + s_kpszEnDash + L" 16 KB)",      RowGrouping::GetSizeGroup (false, 1).label);
        Assert::AreEqual (std::wstring (L"Tiny (0 ") + s_kpszEnDash + L" 16 KB)",      RowGrouping::GetSizeGroup (false, 16 * kKiB).label);
        Assert::AreEqual (std::wstring (L"Small (16 KB ") + s_kpszEnDash + L" 1 MB)",  RowGrouping::GetSizeGroup (false, 16 * kKiB + 1).label);
        Assert::AreEqual (std::wstring (L"Small (16 KB ") + s_kpszEnDash + L" 1 MB)",  RowGrouping::GetSizeGroup (false, kMiB).label);
        Assert::AreEqual (std::wstring (L"Medium (1 ") + s_kpszEnDash + L" 128 MB)",   RowGrouping::GetSizeGroup (false, kMiB + 1).label);
        Assert::AreEqual (std::wstring (L"Large (128 MB ") + s_kpszEnDash + L" 1 GB)", RowGrouping::GetSizeGroup (false, kGiB).label);
        Assert::AreEqual (std::wstring (L"Huge (1 ") + s_kpszEnDash + L" 4 GB)",       RowGrouping::GetSizeGroup (false, 4 * kGiB).label);
        Assert::AreEqual (std::wstring (L"Gigantic (>4 GB)"),                          RowGrouping::GetSizeGroup (false, 4 * kGiB + 1).label);
    }



    TEST_METHOD (Names_ByFirstCharacter)
    {
        Assert::AreEqual (std::wstring (L"0 ") + s_kpszEnDash + L" 9", RowGrouping::GetNameGroup (L"7up").label);
        Assert::AreEqual (std::wstring (L"A ") + s_kpszEnDash + L" H", RowGrouping::GetNameGroup (L"apple").label);
        Assert::AreEqual (std::wstring (L"I ") + s_kpszEnDash + L" P", RowGrouping::GetNameGroup (L"Iris").label);
        Assert::AreEqual (std::wstring (L"Q ") + s_kpszEnDash + L" Z", RowGrouping::GetNameGroup (L"zeta").label);
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
