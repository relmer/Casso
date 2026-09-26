#include "Pch.h"

#include "Widgets/DxuiTextView.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextViewTests
//
//  Columns, the hanging indent of a wrapped last cell, character selection
//  across cells and rows, and scrolling. Cells are 8 by 16 pixels at 96 DPI,
//  inside a 6-pixel pad.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextViewTests)
{
public:

    static DxuiTextView::Row  MakeRow (std::vector<std::wstring> cells, bool warning = false)
    {
        DxuiTextView::Row  row;

        row.cells   = std::move (cells);
        row.warning = warning;

        return row;
    }


    static void  LayOut (DxuiTextView & view, LONG width, LONG height)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        view.SetCellSize (8, 16);
        view.Layout (RECT { 0, 0, width, height }, scaler);
    }


    static void  Mouse (DxuiTextView & view, DxuiMouseEventKind kind, int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.kind        = kind;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };

        view.OnMouse (ev);
    }


    TEST_METHOD (LastCell_WrapsAtASpaceUnderItsOwnFirstCharacter)
    {
        DxuiTextView  view;

        LayOut (view, 12 + 16 * 8, 400);
        view.SetRows ({ MakeRow ({ L"10", L"PRINT AAAA BBBB CCCC" }) });

        Assert::AreEqual (2, view.GetLineCount(), L"Sixteen columns less the number and its gap leave twelve, so the statement wraps once");

        //  The second line's fifth column is the first character after the
        //  number's column and gap, which is where the wrapped run begins.
        Assert::AreEqual (14, view.HitTest (POINT { 6 + 4 * 8, 6 + 16 + 4 }).offset,
            L"and the wrapped run starts with BBBB, under the P of PRINT");
    }


    TEST_METHOD (SelectionText_SpansCellsAndRows)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) });

        view.Select (DxuiTextView::Position { 0, 2 }, DxuiTextView::Position { 1, 3 });

        Assert::AreEqual (std::wstring (L"one\r\nB\tt"), view.GetSelectionText(),
            L"A tab separates cells and a line break separates rows");
    }


    TEST_METHOD (Drag_SelectsFromTheCharacterPressedToTheOneReleased)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) });

        Mouse (view, DxuiMouseEventKind::Down, 6 + 3 * 8, 6 + 4);
        Mouse (view, DxuiMouseEventKind::Move, 6 + 5 * 8, 6 + 16 + 4);
        Mouse (view, DxuiMouseEventKind::Up,   6 + 5 * 8, 6 + 16 + 4);

        Assert::AreEqual (std::wstring (L"one\r\nB\ttw"), view.GetSelectionText());
        Assert::IsFalse  (view.IsInteracting());
    }


    TEST_METHOD (Commands_CopyNeedsASelection_SelectAllTakesEverything)
    {
        DxuiTextView  view;
        bool          enabled = true;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"A", L"one" }), MakeRow ({ L"B", L"two" }) });

        Assert::IsTrue   (view.QueryCommand (DxuiStandardCommand::Copy, enabled));
        Assert::IsFalse  (enabled, L"Nothing selected, nothing to copy");

        Assert::IsTrue   (view.InvokeCommand (DxuiStandardCommand::SelectAll));
        Assert::AreEqual (std::wstring (L"A\tone\r\nB\ttwo"), view.GetSelectionText());
    }


    TEST_METHOD (Wheel_ScrollsWhenTheLinesOutnumberTheView)
    {
        DxuiTextView                    view;
        std::vector<DxuiTextView::Row>  rows;
        DxuiMouseEvent                  ev;

        for (int i = 0; i < 100; i++)
        {
            rows.push_back (MakeRow ({ std::to_wstring (i) }));
        }

        LayOut (view, 400, 12 + 16 * 10);
        view.SetRows (std::move (rows));

        Assert::IsTrue   (view.IsScrollbarVisible());

        ev.kind       = DxuiMouseEventKind::Wheel;
        ev.wheelDelta = -1.0f;
        view.OnMouse (ev);

        Assert::AreEqual (DxuiTextView::kWheelLines, view.GetTopLine());
    }


    TEST_METHOD (WarningRow_StartsItsTextAfterTheMark)
    {
        DxuiTextView  view;

        LayOut (view, 400, 400);
        view.SetRows ({ MakeRow ({ L"Line 10: the file ends" }, true) });

        Assert::AreEqual (0, view.HitTest (POINT { 6 + DxuiTextView::kWarningCells * 8, 6 + 4 }).offset,
            L"The first character is past the warning mark");
    }


    static DxuiTextView::FindResult  Find (const std::vector<std::wstring> & rows, const std::wstring & needle, bool matchCase, bool forward,
                                           DxuiTextView::Position from, DxuiTextView::Position & outStart)
    {
        return DxuiTextView::FindInRows (rows, needle, matchCase, forward, from, outStart);
    }


    TEST_METHOD (Find_MatchesCaseOnlyWhenAsked)
    {
        std::vector<std::wstring>  rows  = { L"lda #$00", L"LDA $0300" };
        DxuiTextView::Position     start;

        Assert::IsTrue   (Find (rows, L"LDA", true, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (1, start.row, L"matching case skips the lowercase lda");
        Assert::AreEqual (0, start.offset);

        Assert::IsTrue   (Find (rows, L"LDA", false, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (0, start.row, L"ignoring case finds the first one");

        Assert::IsTrue   (Find (rows, L"Lda", true, true, {}, start) == DxuiTextView::FindResult::NotFound,
                          L"neither row has it in that case");
    }


    TEST_METHOD (Find_ForwardStartsAtAndBackwardBeforeThePosition)
    {
        std::vector<std::wstring>  rows  = { L"one two one", L"two one" };
        DxuiTextView::Position     start;

        Assert::IsTrue   (Find (rows, L"one", true, true, { 0, 1 }, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (0, start.row);
        Assert::AreEqual (8, start.offset, L"forward, the next one along the row");

        Assert::IsTrue   (Find (rows, L"one", true, true, { 0, 9 }, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (1, start.row,    L"forward, on to the next row");
        Assert::AreEqual (4, start.offset);

        Assert::IsTrue   (Find (rows, L"one", true, false, { 1, 4 }, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (0, start.row,    L"backward, a match at the position itself is not before it");
        Assert::AreEqual (8, start.offset);

        Assert::IsTrue   (Find (rows, L"one", true, false, { 0, 8 }, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (0, start.row);
        Assert::AreEqual (0, start.offset, L"backward, the earlier one on the same row");
    }


    TEST_METHOD (Find_GoesRoundPastEitherEnd)
    {
        std::vector<std::wstring>  rows  = { L"BRK", L"NOP", L"BRK" };
        DxuiTextView::Position     start;

        Assert::IsTrue   (Find (rows, L"BRK", true, true, { 2, 1 }, start) == DxuiTextView::FindResult::Wrapped,
                          L"forward past the last one goes round to the top");
        Assert::AreEqual (0, start.row);

        Assert::IsTrue   (Find (rows, L"BRK", true, false, { 0, 0 }, start) == DxuiTextView::FindResult::Wrapped,
                          L"backward past the first one goes round to the bottom");
        Assert::AreEqual (2, start.row);

        Assert::IsTrue   (Find (rows, L"NOP", true, true, { 1, 1 }, start) == DxuiTextView::FindResult::Wrapped,
                          L"the only match, found again from just past it");
        Assert::AreEqual (1, start.row);
    }


    TEST_METHOD (Find_NoMatchOrNothingToFind)
    {
        std::vector<std::wstring>  rows  = { L"LDA #$00", L"RTS" };
        DxuiTextView::Position     start = { 7, 7 };

        Assert::IsTrue   (Find (rows, L"JMP", false, true,  {}, start) == DxuiTextView::FindResult::NotFound);
        Assert::IsTrue   (Find (rows, L"JMP", false, false, {}, start) == DxuiTextView::FindResult::NotFound);
        Assert::IsTrue   (Find (rows, L"",    false, true,  {}, start) == DxuiTextView::FindResult::NotFound, L"an empty needle matches nothing");
        Assert::IsTrue   (Find ({},   L"RTS", false, true,  {}, start) == DxuiTextView::FindResult::NotFound, L"no text");
        Assert::AreEqual (7, start.row, L"a failed search leaves the answer alone");
    }


    TEST_METHOD (SelectMatch_SelectsEachMatchInTurnAndScrollsItIntoView)
    {
        DxuiTextView                    view;
        std::vector<DxuiTextView::Row>  rows;

        for (int i = 0; i < 100; i++)
        {
            rows.push_back (MakeRow ({ (i == 5 || i == 80) ? L"STA $C030" : L"NOP" }));
        }

        LayOut (view, 400, 12 + 16 * 10);
        view.SetRows (std::move (rows));

        Assert::IsTrue   (view.SelectMatch (L"sta", false, true) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (std::wstring (L"STA"), view.GetSelectionText(), L"the match is selected");
        Assert::AreEqual (0, view.GetTopLine(), L"row 5 is already in view");

        Assert::IsTrue   (view.SelectMatch (L"sta", false, true) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (std::wstring (L"STA"), view.GetSelectionText());
        Assert::AreEqual (80 - 10 + 1, view.GetTopLine(), L"row 80 is scrolled to the bottom of the view");

        Assert::IsTrue   (view.SelectMatch (L"sta", false, true) == DxuiTextView::FindResult::Wrapped);
        Assert::AreEqual (5, view.GetTopLine(), L"round to row 5, scrolled back up to it");

        Assert::IsTrue   (view.SelectMatch (L"sta", false, false) == DxuiTextView::FindResult::Wrapped);
        Assert::AreEqual (80 - 10 + 1, view.GetTopLine(), L"backward from row 5 goes round to row 80");

        Assert::IsTrue   (view.SelectMatch (L"LDA", false, true) == DxuiTextView::FindResult::NotFound);
        Assert::AreEqual (std::wstring (L"STA"), view.GetSelectionText(), L"a failed search keeps the selection");
    }


    TEST_METHOD (SelectMatch_ScrollsToTheWrappedLineAMatchIsOn)
    {
        DxuiTextView                    view;
        std::vector<DxuiTextView::Row>  rows;

        for (int i = 0; i < 20; i++)
        {
            rows.push_back (MakeRow ({ L"NOP" }));
        }

        //  The last row wraps over several lines, and the match is on its last.
        rows.push_back (MakeRow ({ L"AAAA BBBB CCCC DDDD EEEE FFFF GGGG HHHH IIII KKKK JUMP" }));

        LayOut (view, 12 + 16 * 8, 12 + 16 * 4);
        view.SetRows (std::move (rows));

        Assert::IsTrue   (view.GetLineCount() - view.GetFirstLineOfRow (20) > 4, L"the row wraps over more lines than the view shows");

        Assert::IsTrue   (view.SelectMatch (L"JUMP", true, true) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (view.GetLineCount() - 4, view.GetTopLine(), L"the match's line shows, at the bottom of the view, not the row's first");
    }
};
