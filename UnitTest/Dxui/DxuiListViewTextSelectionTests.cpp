#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewTextSelectionTests
//
//  A list that reads as text: a drag selects characters across cells and
//  rows, a click without a drag selects nothing but its row, a double-click
//  selects a word, Copy takes the characters, and a selection clears when the
//  text under it changes.
//
//  No paint runs here, so the list places characters at its fixed advance
//  rather than a measured one; the points below are computed the same way.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewTextSelectionTests)
{
public:

    static constexpr float  kAdvancePerDip = 0.6f;



    static void  ConfigureList (DxuiListView & list)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<DxuiListView::Column>             cols;
        std::vector<std::vector<DxuiListView::Cell>>  data;

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column { L"Op",      80 });
        cols.push_back (DxuiListView::Column { L"Operand", 120 });

        data.push_back ({ DxuiListView::Cell { L"LDA" }, DxuiListView::Cell { L"$0400" } });
        data.push_back ({ DxuiListView::Cell { L"STA" }, DxuiListView::Cell { L"$0402" } });

        list.SetColumns              (std::move (cols));
        list.SetRows                 (std::move (data));
        list.SetActivateOnDoubleClick (true);
        list.SetTextSelection         (true);
        list.Layout                   (RECT { 0, 0, 400, 300 }, scaler);
    }


    //  A point just inside character `ch` of a cell, left of its middle, so
    //  it lands on the boundary before that character.
    static POINT  At (const DxuiListView & list, int row, size_t col, int ch)
    {
        RECT   cell    = {};
        float  advance = list.GetFontSizeDip() * kAdvancePerDip;

        Assert::IsTrue (list.GetCellTextRectPx (row, col, cell));

        return POINT { cell.left + (int) (advance * (float) ch + advance * 0.25f), (cell.top + cell.bottom) / 2 };
    }


    static void  Send (DxuiListView & list, DxuiMouseEventKind kind, POINT at)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = at;

        (void) list.OnMouse (ev);
    }


    TEST_METHOD (ADragSelectsCharactersAcrossCellsAndRows)
    {
        DxuiListView  list;

        ConfigureList (list);

        Send (list, DxuiMouseEventKind::Down, At (list, 0, 0, 1));
        Send (list, DxuiMouseEventKind::Move, At (list, 1, 1, 3));
        Send (list, DxuiMouseEventKind::Up,   At (list, 1, 1, 3));

        Assert::IsTrue   (list.HasTextSelection());
        Assert::AreEqual (std::wstring (L"DA\t$0400\r\nSTA\t$04"), list.GetSelectionText(),
                          L"from D in LDA to after $04 in the next row, a tab between cells");
    }


    TEST_METHOD (ADragBackwardSelectsTheSameText)
    {
        DxuiListView  list;

        ConfigureList (list);

        Send (list, DxuiMouseEventKind::Down, At (list, 1, 1, 3));
        Send (list, DxuiMouseEventKind::Move, At (list, 0, 0, 1));
        Send (list, DxuiMouseEventKind::Up,   At (list, 0, 0, 1));

        Assert::AreEqual (std::wstring (L"DA\t$0400\r\nSTA\t$04"), list.GetSelectionText());
    }


    TEST_METHOD (AClickWithoutADragSelectsItsRowAndNoText)
    {
        DxuiListView  list;

        ConfigureList (list);

        Send (list, DxuiMouseEventKind::Down, At (list, 0, 0, 1));
        Send (list, DxuiMouseEventKind::Move, At (list, 1, 1, 3));
        Send (list, DxuiMouseEventKind::Up,   At (list, 1, 1, 3));

        //  A second click drops the selection and picks its row.
        Send (list, DxuiMouseEventKind::Down, At (list, 1, 0, 0));
        Send (list, DxuiMouseEventKind::Up,   At (list, 1, 0, 0));

        Assert::IsFalse  (list.HasTextSelection());
        Assert::AreEqual (1, list.GetSelectedRow());
        Assert::AreEqual (std::wstring (L"STA\t$0402\r\n"), list.GetSelectionText(), L"with no text selected, Copy takes the row");
    }


    TEST_METHOD (ADoubleClickSelectsTheWordUnderThePointer)
    {
        DxuiListView  list;

        ConfigureList (list);

        for (int click = 0; click < 2; click++)
        {
            Send (list, DxuiMouseEventKind::Down, At (list, 0, 1, 2));
            Send (list, DxuiMouseEventKind::Up,   At (list, 0, 1, 2));
        }

        Assert::IsTrue   (list.HasTextSelection());
        Assert::AreEqual (std::wstring (L"$0400"), list.GetSelectionText(), L"the $ is part of the number");
    }


    TEST_METHOD (TheSelectionClearsOnlyWhenTheTextUnderItChanges)
    {
        DxuiListView                                  list;
        std::vector<std::vector<DxuiListView::Cell>>  same;
        std::vector<std::vector<DxuiListView::Cell>>  changed;

        ConfigureList (list);

        Send (list, DxuiMouseEventKind::Down, At (list, 0, 0, 0));
        Send (list, DxuiMouseEventKind::Move, At (list, 0, 1, 2));
        Send (list, DxuiMouseEventKind::Up,   At (list, 0, 1, 2));

        same.push_back ({ DxuiListView::Cell { L"LDA" }, DxuiListView::Cell { L"$0400" } });
        same.push_back ({ DxuiListView::Cell { L"STA" }, DxuiListView::Cell { L"$0402" } });
        list.SetRows (std::move (same));
        Assert::IsTrue (list.HasTextSelection(), L"the same rows again, as a paused debugger resends them");

        changed.push_back ({ DxuiListView::Cell { L"JMP" }, DxuiListView::Cell { L"$E000" } });
        changed.push_back ({ DxuiListView::Cell { L"STA" }, DxuiListView::Cell { L"$0402" } });
        list.SetRows (std::move (changed));
        Assert::IsFalse (list.HasTextSelection(), L"the text under the selection is gone");
    }
};
