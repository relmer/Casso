#include "Pch.h"
#include "CassoExplorer/Model/CassoExplorerPrefs.h"
#include "CassoExplorer/Model/ChooseDetailsModel.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  ChooseDetailsModelTests
//
//  Choose Details with no window: the entries listed for the list's columns
//  and the shell's properties, what moving, checking and a width make of
//  them, the sort by a property's values, and the properties saved in the
//  preferences.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (ChooseDetailsModelTests)
{
public:

    //  Name, Date modified and Type in that order on screen, Size hidden,
    //  and Address not offered here; two properties, one already a column.
    static ChooseDetailsModel MakeModel()
    {
        ChooseDetailsModel                           model;
        std::vector<ChooseDetailsModel::Column>      columns (5);
        std::vector<Win32PropertyColumns::Property>  properties (2);

        columns[0] = { L"Name", true, true, true, 250 };
        columns[1] = { L"Date modified", true, true, false, 144 };
        columns[2] = { L"Type", true, true, false, 120 };
        columns[3] = { L"Size", true, false, false, 80 };
        columns[4] = { L"Address", false, true, false, 72 };

        properties[0] = { L"System.Author", L"Authors", 140 };
        properties[1] = { L"System.Size",   L"Size",    80 };

        model.Build (columns, { 0, 2, 1, 3, 4 }, { L"System.Size" }, properties);

        return model;
    }



    TEST_METHOD (Build_ListsTheColumnsInOrder_ThenTheOtherProperties)
    {
        ChooseDetailsModel                              model   = MakeModel();
        const std::vector<ChooseDetailsModel::Entry> &  entries = model.GetEntries();

        Assert::AreEqual ((size_t) 5, entries.size(), L"Address isn't offered, and Size isn't listed twice");
        Assert::AreEqual (std::wstring (L"Type"),    entries[1].title);
        Assert::IsFalse  (entries[3].checked, L"Size is hidden");
        Assert::AreEqual (std::wstring (L"Authors"), entries[4].title);
        Assert::AreEqual (-1, entries[4].column);
        Assert::IsTrue   (entries[0].fixed);
    }



    TEST_METHOD (Moves_KeepNameFirst)
    {
        ChooseDetailsModel  model = MakeModel();

        Assert::IsFalse (model.CanMoveUp (0));
        Assert::IsFalse (model.CanMoveUp (1), L"Nothing goes above Name");
        Assert::IsTrue  (model.CanMoveDown (1));

        model.MoveDown (1);
        Assert::AreEqual (std::wstring (L"Date modified"), model.GetEntries()[1].title);

        model.Toggle (0);
        Assert::IsTrue (model.GetEntries()[0].checked, L"Name stays checked");
    }



    //  A checked property is added after the last column; the column not
    //  offered goes after the listed ones, and its choice is copied.
    TEST_METHOD (Result_AddsCheckedProperties_AndKeepsTheRest)
    {
        ChooseDetailsModel          model = MakeModel();
        ChooseDetailsModel::Result  result;

        model.Toggle   (4);
        model.Toggle   (2);
        model.SetWidth (1, 200);
        result = model.GetResult();

        Assert::AreEqual ((size_t) 1, result.added.size());
        Assert::AreEqual (std::wstring (L"System.Author"), result.added[0]);
        Assert::IsTrue   (result.order == std::vector<int> ({ 0, 2, 1, 3, 5, 4 }));
        Assert::IsFalse  (result.chosen[1], L"Date modified hidden");
        Assert::IsTrue   (result.chosen[5], L"Authors visible");
        Assert::IsTrue   (result.chosen[4], L"Address's choice is copied");
        Assert::IsTrue   (std::find (result.widths.begin(), result.widths.end(), std::pair<int, int> (2, 200)) != result.widths.end(), L"Type's new width");
        Assert::IsTrue   (std::find (result.widths.begin(), result.widths.end(), std::pair<int, int> (5, 140)) != result.widths.end(), L"Authors' default width");
    }



    //  Folders first, numbers by value, text by its words and numbers, and a
    //  row without the property ahead of the rest.
    TEST_METHOD (SortByProperty_OrdersByNumberOrText)
    {
        std::vector<CatalogRow>                 rows (4);
        std::vector<CatalogModel::PropertyKey>  keys (4);

        rows[0].name = L"a";
        rows[1].name = L"b";
        rows[2].name = L"c";
        rows[3].name = L"dir";
        rows[3].isDirectory = true;

        keys[0] = { true, true, 10.0, L"10" };
        keys[1] = { true, true, 9.0,  L"9" };
        keys[2] = {};
        keys[3] = { true, true, 50.0, L"50" };

        CatalogModel::SortByProperty (rows, keys, false);
        Assert::IsTrue (rows[0].name == L"dir" && rows[1].name == L"c" && rows[2].name == L"b" && rows[3].name == L"a");

        keys = { CatalogModel::PropertyKey { true, false, 0.0, L"Smith" }, CatalogModel::PropertyKey { true, false, 0.0, L"Jones" } };
        rows.resize (2);
        rows[0].name        = L"x";
        rows[1].name        = L"y";
        rows[0].isDirectory = false;
        rows[1].isDirectory = false;

        CatalogModel::SortByProperty (rows, keys, true);
        Assert::AreEqual (std::wstring (L"x"), rows[0].name, L"Descending, Smith first");
    }



    TEST_METHOD (Prefs_KeepThePropertyColumns)
    {
        CassoExplorerPrefs  saved;
        CassoExplorerPrefs  loaded;

        saved.propertyColumns = { L"System.Author", L"System.Image.Dimensions" };
        AssertSucceeded (loaded.FromJson (saved.ToJson()));

        Assert::IsTrue (loaded.propertyColumns == saved.propertyColumns);
    }
};
