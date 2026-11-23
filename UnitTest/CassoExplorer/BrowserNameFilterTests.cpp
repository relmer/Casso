#include "Pch.h"

#include "CassoExplorer/Browser/BrowserNameFilter.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserNameFilterTests
//
//  A picker's name filter over rows built in memory.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserNameFilterTests)
{
public:

    static CatalogRow MakeRow (const wchar_t * name)
    {
        CatalogRow  row;

        row.name = name;

        return row;
    }



    TEST_METHOD (NameFilterWords)
    {
        std::vector<std::wstring>  words = BrowserNameFilter::SplitFilterWords (L"  Zork   II ");



        Assert::AreEqual ((size_t) 2, words.size());
        Assert::AreEqual (std::wstring (L"zork"), words[0]);
        Assert::AreEqual (std::wstring (L"ii"),   words[1]);
        Assert::IsTrue  (BrowserNameFilter::IsNameFilterMatch (L"Zork II.woz", words));
        Assert::IsFalse (BrowserNameFilter::IsNameFilterMatch (L"Zork I.woz",  words));
        Assert::IsTrue  (BrowserNameFilter::IsNameFilterMatch (L"anything",    {}));
    }



    TEST_METHOD (Apply_ListsTheMatches_AndTheBaseRowsWithNoText)
    {
        BrowserNameFilter        filter;
        std::vector<CatalogRow>  rows;



        filter.CaptureSource (Location::MakeHostFolder (L"C:\\Disks"), true, 2);
        filter.SetSource ({ MakeRow (L"Zork I.dsk"), MakeRow (L"Archon.dsk"), MakeRow (L"Zork II.dsk") });
        Assert::IsTrue (filter.IsFilterable());

        filter.Apply (rows);
        Assert::AreEqual ((size_t) 2, rows.size(), L"With no text, the rows past the base are left out");

        Assert::IsTrue (filter.SetText (L"zork"));
        filter.Apply (rows);
        Assert::AreEqual ((size_t) 2, rows.size());
        Assert::AreEqual (std::wstring (L"Zork II.dsk"), rows[1].name);

        Assert::IsFalse (filter.SetText (L"zork"), L"The same text again changes nothing");
    }



    TEST_METHOD (CaptureSource_NoScopeOrAnImage_IsNotFilterable)
    {
        BrowserNameFilter  filter;



        filter.CaptureSource (Location::MakeHostFolder (L"C:\\Disks"), false, 1);
        Assert::IsFalse (filter.IsFilterable());

        filter.CaptureSource (Location::MakeDiskImage (L"C:\\Disks\\a.dsk"), true, 1);
        Assert::IsFalse (filter.IsFilterable());
        Assert::IsFalse (filter.SetText (L"a"), L"The text is set, but no rows are filtered");
        Assert::AreEqual (std::wstring (L"a"), filter.GetText());
    }
};
