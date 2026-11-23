#include "Pch.h"

#include "CassoExplorer/Browser/BrowserColumnFilters.h"
#include "CassoExplorer/Model/CatalogModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserColumnFiltersTests
//
//  The column filters over rows built in memory: the choices of a column,
//  the rows left in a checked group, and the filters ending when the folder
//  changes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserColumnFiltersTests)
{
public:

    static CatalogRow MakeRow (const wchar_t * name, bool isDirectory, uint64_t bytes)
    {
        CatalogRow  row;

        row.name        = name;
        row.isDirectory = isDirectory;
        row.sizeBytes   = bytes;

        return row;
    }


    static std::vector<CatalogRow> MakeRows()
    {
        return { MakeRow (L"zebra", false, 100), MakeRow (L"apple", false, 200), MakeRow (L"folder", true, 0) };
    }


    TEST_METHOD (GetFilterField_CoversTheGroupedColumnsOnly)
    {
        Assert::IsTrue (BrowserColumnFilters::GetFilterField ((size_t) CatalogModel::Column::Name)    == RowGrouping::Field::Name);
        Assert::IsTrue (BrowserColumnFilters::GetFilterField ((size_t) CatalogModel::Column::Type)    == RowGrouping::Field::Type);
        Assert::IsTrue (BrowserColumnFilters::GetFilterField ((size_t) CatalogModel::Column::Address) == RowGrouping::Field::None);
    }


    TEST_METHOD (FilterRows_KeepsTheCheckedGroups_AndOffersEveryRowsGroup)
    {
        BrowserColumnFilters     filters;
        std::vector<CatalogRow>  rows     = MakeRows();
        Location                 folder   = Location::MakeHostFolder (L"C:\\Disks");
        std::wstring             label;

        filters.MarkRowsLoaded();
        filters.FilterRows (rows, folder);

        Assert::AreEqual ((size_t) 3, rows.size(), L"Without a filter, every row is listed");
        Assert::IsTrue   (filters.GetFilterChoices (RowGrouping::Field::Name).size() >= 2);

        label = RowGrouping::GetGroup (rows[1], RowGrouping::Field::Name, RowGrouping::GetToday()).label;
        filters.ToggleFilter (RowGrouping::Field::Name, label);
        Assert::IsTrue (filters.IsFiltering (RowGrouping::Field::Name));
        Assert::IsTrue (filters.IsFilterChecked (RowGrouping::Field::Name, label));

        rows = MakeRows();
        filters.MarkRowsLoaded();
        filters.FilterRows (rows, folder);

        Assert::IsTrue (rows.size() < 3);

        for (const CatalogRow & row : rows)
        {
            Assert::AreEqual (label, RowGrouping::GetGroup (row, RowGrouping::Field::Name, RowGrouping::GetToday()).label);
        }

        Assert::IsTrue (filters.GetFilterChoices (RowGrouping::Field::Name).size() >= 2, L"The choices include the rows filtered out");
    }


    TEST_METHOD (ToggleFilter_TheLastGroupUncheckedEndsTheFilter)
    {
        BrowserColumnFilters  filters;

        filters.ToggleFilter (RowGrouping::Field::Type, L"Text");
        Assert::IsTrue (filters.IsFiltering (RowGrouping::Field::Type));

        filters.ToggleFilter (RowGrouping::Field::Type, L"Text");
        Assert::IsFalse (filters.IsFiltering (RowGrouping::Field::Type));

        filters.ToggleFilter (RowGrouping::Field::Type, L"Text");
        filters.ClearFilter  (RowGrouping::Field::Type);
        Assert::IsFalse (filters.IsFiltering (RowGrouping::Field::Type));
    }


    TEST_METHOD (FilterRows_AnotherFolderStartsUnfiltered)
    {
        BrowserColumnFilters     filters;
        std::vector<CatalogRow>  rows = MakeRows();

        filters.MarkRowsLoaded();
        filters.FilterRows (rows, Location::MakeHostFolder (L"C:\\Disks"));
        filters.ToggleFilter (RowGrouping::Field::Size, L"Unspecified");

        rows = MakeRows();
        filters.MarkRowsLoaded();
        filters.FilterRows (rows, Location::MakeHostFolder (L"C:\\Other"));

        Assert::IsFalse  (filters.IsFiltering (RowGrouping::Field::Size));
        Assert::AreEqual ((size_t) 3, rows.size());
    }


    TEST_METHOD (FilterRows_ListsUnspecifiedLast)
    {
        BrowserColumnFilters     filters;
        std::vector<CatalogRow>  rows = MakeRows();

        filters.MarkRowsLoaded();
        filters.FilterRows (rows, Location::MakeHostFolder (L"C:\\Disks"));

        Assert::AreEqual (std::wstring (L"Unspecified"), filters.GetFilterChoices (RowGrouping::Field::Size).back().label);
    }


    TEST_METHOD (FilterRows_RowsNotReloaded_AreNotFilteredAgain)
    {
        BrowserColumnFilters     filters;
        std::vector<CatalogRow>  rows = MakeRows();

        filters.ToggleFilter (RowGrouping::Field::Size, L"Unspecified");
        filters.FilterRows (rows, Location());

        Assert::AreEqual ((size_t) 3, rows.size());
    }
};
