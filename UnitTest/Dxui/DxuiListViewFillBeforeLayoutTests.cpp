#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewFillBeforeLayoutTests
//
//  A list filled before its first layout opens at its first row.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewFillBeforeLayoutTests)
{
public:

    TEST_METHOD (AListFilledBeforeLayout_OpensAtTheTop)
    {
        DxuiListView                                  list;
        DxuiDpiScaler                                 scaler;
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        int                                           i = 0;

        scaler.SetDpi (96);

        for (i = 0; i < 200; i++)
        {
            rows.push_back ({ DxuiListView::Cell { L"row", false } });
        }

        list.SetColumns (std::vector<DxuiListView::Column> { DxuiListView::Column { L"Name", 200 } });
        list.SetRows    (std::move (rows));
        list.Layout     (RECT { 0, 0, 400, 300 }, scaler);

        Assert::AreEqual (0, list.GetTopRow());
    }
};
