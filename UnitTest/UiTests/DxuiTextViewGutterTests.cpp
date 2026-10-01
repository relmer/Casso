#include "Pch.h"

#include "Widgets/DxuiTextView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewGutterTests
//
//  A gutter ahead of the text, for each row's icon: the text starts past it,
//  so a point maps to the character drawn under it and a line wraps short of
//  the right edge by the gutter's width. Cells are 8 by 16 pixels at 96 DPI,
//  inside a 6-pixel pad.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextViewGutterTests)
{
public:

    static void  LayOut (DxuiTextView & view, LONG width, LONG height)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        view.SetCellSize (8, 16);
        view.Layout (RECT { 0, 0, width, height }, scaler);
    }


    TEST_METHOD (TheTextStartsPastTheGutter)
    {
        DxuiTextView       view;
        DxuiTextView::Row  row;



        row.cells = { L"abcdef" };
        view.SetGutter (24, 16);
        LayOut (view, 400, 400);
        view.SetRows ({ row });

        Assert::AreEqual (0, view.HitTest (POINT { 6 + 24, 6 + 4 }).offset);
        Assert::AreEqual (2, view.HitTest (POINT { 6 + 24 + 2 * 8, 6 + 4 }).offset);
    }


    TEST_METHOD (ALineWrapsShortOfTheGutter)
    {
        DxuiTextView       view;
        DxuiTextView::Row  row;



        //  Sixteen columns of width, less three for the gutter, leave thirteen.
        row.cells = { L"aaaaaa bbbbbb cc" };
        view.SetGutter (24, 16);
        LayOut (view, 12 + 16 * 8, 400);
        view.SetRows ({ row });

        Assert::AreEqual (2, view.GetLineCount());
    }
};
