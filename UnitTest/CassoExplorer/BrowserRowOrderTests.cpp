#include "Pch.h"

#include "CassoExplorer/Browser/BrowserColumnFilters.h"
#include "CassoExplorer/Browser/BrowserRowOrder.h"
#include "CassoExplorer/Browser/IBrowserListing.h"
#include "CassoExplorer/Model/BrowserModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BrowserRowOrderTests
//
//  The rows' sort, Group by and Home sections over rows built in memory and
//  a fake listing that reports whether the location is Home.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (BrowserRowOrderTests)
{
public:

    class FakeListing : public IBrowserListing
    {
    public:
        void      ReloadAfterNavigation() override                                           {}
        void      ReturnToTab           (const std::vector<std::wstring> &) override         {}
        bool      IsOnMissingDrive      (const Location &) const override                    { return false; }
        bool      IsHomeLocation() const override                                            { return home; }
        void      RequestHomeRead() override                                                 {}
        Location  GetShellParent() const override                                            { return Location(); }

        bool  home = false;
    };


    struct Rig
    {
        BrowserModel             model;
        std::vector<CatalogRow>  rows;
        std::vector<int>         selected;
        BrowserColumnFilters     filters;
        FakeListing              listing;
        BrowserRowOrder          order     { model, rows, selected, filters, listing };

        Rig()
        {
            model.OpenTab (Location::MakeHostFolder (L"C:\\Disks"));
        }

        void AddRow (const wchar_t * name, int homeGroup = 0)
        {
            CatalogRow  row;

            row.name        = name;
            row.hostPath    = std::wstring (L"C:\\Disks\\") + name;
            row.homeGroup   = homeGroup;
            row.sourceIndex = rows.size();

            rows.push_back (row);
        }

        std::wstring GetRowText() const
        {
            std::wstring  text;

            for (const CatalogRow & row : rows)
            {
                text += row.name + L" ";
            }

            return text;
        }
    };



    TEST_METHOD (SortByColumn_SameColumnAgain_Reverses_AndSelectionFollows)
    {
        Rig  rig;



        rig.AddRow (L"b");
        rig.AddRow (L"a");
        rig.AddRow (L"c");
        rig.order.SortRows();
        Assert::AreEqual (std::wstring (L"a b c "), rig.GetRowText());

        rig.selected = { 0 };
        rig.order.SortByColumn ((int) CatalogModel::Column::Name);

        Assert::IsTrue   (rig.model.GetActiveTab().sortDescending);
        Assert::AreEqual (std::wstring (L"c b a "), rig.GetRowText());
        Assert::AreEqual ((size_t) 1, rig.selected.size());
        Assert::AreEqual (std::wstring (L"a"), rig.rows[(size_t) rig.selected[0]].name);
    }



    TEST_METHOD (SortByColumn_UnknownColumn_LeavesTheSort)
    {
        Rig  rig;



        rig.order.SortByColumn ((int) CatalogModel::kColumnCount);

        Assert::IsTrue  (rig.model.GetActiveTab().sortColumn == CatalogModel::Column::Name);
        Assert::IsFalse (rig.model.GetActiveTab().sortDescending);
    }



    TEST_METHOD (IsPropertyColumn_CoversTheColumnsAfterTheFixedOnes)
    {
        Rig  rig;



        Assert::IsFalse (rig.order.IsPropertyColumn ((int) CatalogModel::kColumnCount));

        rig.order.SetPropertyColumns (2, [] (size_t, size_t) { return CatalogModel::PropertyKey(); });

        Assert::IsTrue  (rig.order.IsPropertyColumn ((int) CatalogModel::kColumnCount));
        Assert::IsTrue  (rig.order.IsPropertyColumn ((int) CatalogModel::kColumnCount + 1));
        Assert::IsFalse (rig.order.IsPropertyColumn ((int) CatalogModel::kColumnCount + 2));
        Assert::IsFalse (rig.order.IsPropertyColumn ((int) CatalogModel::Column::Name));
    }



    TEST_METHOD (SetGroupBy_ListsAGroupPerRange_AndNoneEndsIt)
    {
        Rig                               rig;
        std::vector<DxuiListView::Group>  groups;



        rig.AddRow (L"Zork");
        rig.AddRow (L"Apple");
        rig.AddRow (L"Moon");

        rig.order.SetGroupBy (RowGrouping::Field::Name, false);
        groups = rig.order.GetListGroups();

        Assert::AreEqual ((size_t) 3, groups.size());
        Assert::AreEqual (0, groups[0].firstRow);
        Assert::AreEqual (std::wstring (L"Apple"), rig.rows[0].name);

        rig.order.SetGroupBy (RowGrouping::Field::None, false);
        Assert::IsTrue (rig.order.GetListGroups().empty());
    }



    TEST_METHOD (Home_ListsQuickAccessThenTheTab_InTwoSections)
    {
        Rig                               rig;
        std::vector<CatalogRow>           home;
        std::vector<DxuiListView::Group>  groups;



        rig.listing.home = true;
        rig.AddRow (L"Recent", 3);
        rig.AddRow (L"Pinned", 1);
        rig.AddRow (L"Shared", 4);
        rig.order.SetHomeRows (rig.rows);

        rig.order.SortRows();
        groups = rig.order.GetListGroups();

        Assert::IsTrue   (rig.order.IsHomeSectioned());
        Assert::AreEqual (std::wstring (L"Pinned Recent "), rig.GetRowText());
        Assert::AreEqual ((size_t) 2, groups.size());
        Assert::AreEqual (1, groups[1].firstRow);

        rig.order.SetHomeTab (2);
        rig.order.SortRows();
        Assert::AreEqual (std::wstring (L"Pinned Shared "), rig.GetRowText());

        rig.order.RemoveHomeRow (L"C:\\Disks\\Shared");
        rig.order.SortRows();
        Assert::AreEqual (std::wstring (L"Pinned "), rig.GetRowText());
    }
};
