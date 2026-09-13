#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewStretchColumnTests
//
//  A stretch column fills the width the other columns leave, but is never
//  narrower than its declared width. Previously, in a pane narrower than the
//  fixed columns it got zero width while the horizontal scroll range still
//  included its declared width, so Cassque's file list opened without its Name
//  column.
//
//  96 DPI, so a DIP is a pixel.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewStretchColumnTests)
{
public:

    static RECT  MakeRect (LONG width)
    {
        RECT  out = {};

        out.right  = width;
        out.bottom = 300;
        return out;
    }


    static void  LayOut (DxuiListView & list, int nameWidthDip, LONG paneWidth)
    {
        DxuiDpiScaler                      scaler;
        std::vector<DxuiListView::Column>  cols;

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column { L"Name", nameWidthDip, true  });
        cols.push_back (DxuiListView::Column { L"Type", 70,           false });
        cols.push_back (DxuiListView::Column { L"Size", 92,           false });

        list.SetColumns    (std::move (cols));
        list.SetShowHeader (false);
        list.Layout        (MakeRect (paneWidth), scaler);
    }


    TEST_METHOD (NarrowPane_StretchColumnKeepsItsDeclaredWidth)
    {
        DxuiListView  list;


        LayOut (list, 200, 150);

        Assert::AreEqual (200, list.GetColumnEffectiveWidthPx (0),
            L"A pane too narrow for the fixed columns still draws the stretch column at its declared width");
    }


    TEST_METHOD (WidePane_StretchColumnStillFillsTheRest)
    {
        DxuiListView  list;
        int           total = 0;


        LayOut (list, 200, 1000);

        total = list.GetColumnEffectiveWidthPx (0)
              + list.GetColumnEffectiveWidthPx (1)
              + list.GetColumnEffectiveWidthPx (2);

        Assert::IsTrue   (list.GetColumnEffectiveWidthPx (0) > 200,
            L"With room to spare the stretch column grows past its declared width");
        Assert::AreEqual (1000, total,
            L"and the columns fill the pane exactly");
    }


    TEST_METHOD (StretchColumnDeclaredAtZero_StillShrinksToFit)
    {
        DxuiListView  list;


        LayOut (list, 0, 150);

        Assert::AreEqual (0, list.GetColumnEffectiveWidthPx (0),
            L"A stretch column with no declared width has no floor, as before");
    }
};
