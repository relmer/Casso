#include "Pch.h"


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


    TEST_METHOD (Keys_ScrollOnlyWithoutCtrlOrAlt)
    {
        DxuiTextView                    view;
        std::vector<DxuiTextView::Row>  rows;
        DxuiKeyEvent                    down     = { DxuiKeyEventKind::Down, VK_DOWN, false, false, false, false };
        DxuiKeyEvent                    ctrlDown = { DxuiKeyEventKind::Down, VK_DOWN, false, false, true,  false };
        DxuiKeyEvent                    altEnd   = { DxuiKeyEventKind::Down, VK_END,  false, false, false, true  };

        for (int i = 0; i < 100; i++)
        {
            rows.push_back (MakeRow ({ std::to_wstring (i) }));
        }

        LayOut (view, 400, 12 + 16 * 10);
        view.SetRows (std::move (rows));

        Assert::IsFalse  (view.OnKey (ctrlDown), L"Ctrl+Down is a shortcut, left to the host");
        Assert::IsFalse  (view.OnKey (altEnd),   L"Alt+End is a shortcut, left to the host");
        Assert::AreEqual (0, view.GetTopLine(),  L"and neither scrolls");

        Assert::IsTrue   (view.OnKey (down));
        Assert::AreEqual (1, view.GetTopLine(),  L"Down alone scrolls a line");
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
        return DxuiTextView::FindInRows (rows, needle, matchCase, false, forward, from, outStart);
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


    static DxuiTextView::FindResult  FindWord (const std::vector<std::wstring> & rows, const std::wstring & needle, bool matchCase, bool forward,
                                               DxuiTextView::Position from, DxuiTextView::Position & outStart)
    {
        return DxuiTextView::FindInRows (rows, needle, matchCase, true, forward, from, outStart);
    }


    TEST_METHOD (FindWord_SkipsAMatchInsideALongerWord)
    {
        std::vector<std::wstring>  rows  = { L"STAX STA_1 XSTA 2STA STA" };
        DxuiTextView::Position     start;

        Assert::IsTrue   (Find (rows, L"STA", true, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (0, start.offset, L"without whole words, the start of STAX matches");

        Assert::IsTrue   (FindWord (rows, L"STA", true, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (21, start.offset, L"a letter, an underscore or a digit on either side is part of the word");

        Assert::IsTrue   (FindWord (rows, L"STA", true, false, { 0, 21 }, start) == DxuiTextView::FindResult::Wrapped,
                          L"backward, every earlier one is inside a word, so the search goes round");
        Assert::AreEqual (21, start.offset);

        Assert::IsTrue   (FindWord ({ L"STAX", L"LDSTA" }, L"STA", true, true, {}, start) == DxuiTextView::FindResult::NotFound);
    }


    TEST_METHOD (FindWord_MatchesAtTheLineStartAndEnd)
    {
        std::vector<std::wstring>  rows  = { L"BRK", L"NOP BRK", L"BRK NOP" };
        DxuiTextView::Position     start;

        Assert::IsTrue   (FindWord (rows, L"BRK", true, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (0, start.row,    L"the whole line");
        Assert::AreEqual (0, start.offset);

        Assert::IsTrue   (FindWord (rows, L"BRK", true, true, { 0, 1 }, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (1, start.row,    L"at the line's end");
        Assert::AreEqual (4, start.offset);

        Assert::IsTrue   (FindWord (rows, L"BRK", true, true, { 1, 5 }, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (2, start.row,    L"at the line's start");
        Assert::AreEqual (0, start.offset);

        Assert::IsTrue   (FindWord (rows, L"BRK", true, false, { 2, 0 }, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (1, start.row,    L"backward, at the line's end");
        Assert::AreEqual (4, start.offset);
    }


    TEST_METHOD (FindWord_PunctuationAndTabsEndAWord)
    {
        std::vector<std::wstring>  rows  = { L"$C030,X", L"(ptr),Y", L"1000:\tLDA" };
        DxuiTextView::Position     start;

        Assert::IsTrue   (FindWord (rows, L"C030", true, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (0, start.row);
        Assert::AreEqual (1, start.offset, L"between a dollar sign and a comma");

        Assert::IsTrue   (FindWord (rows, L"ptr", true, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (1, start.row);
        Assert::AreEqual (1, start.offset, L"between parentheses");

        Assert::IsTrue   (FindWord (rows, L"1000", true, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (2, start.row,    L"before a colon");

        Assert::IsTrue   (FindWord (rows, L"LDA", true, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (6, start.offset, L"after the tab between cells");

        Assert::IsTrue   (FindWord (rows, L"C03", true, true, {}, start) == DxuiTextView::FindResult::NotFound,
                          L"a digit after it is still part of the word");
    }


    TEST_METHOD (FindWord_WorksWithAndWithoutMatchingCase)
    {
        std::vector<std::wstring>  rows  = { L"ldax lda", L"LDA" };
        DxuiTextView::Position     start;

        Assert::IsTrue   (FindWord (rows, L"LDA", false, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (0, start.row,    L"ignoring case, the lowercase whole word");
        Assert::AreEqual (5, start.offset, L"not the start of ldax");

        Assert::IsTrue   (FindWord (rows, L"LDA", true, true, {}, start) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (1, start.row,    L"matching case, only the capitals");
        Assert::AreEqual (0, start.offset);

        Assert::IsTrue   (FindWord (rows, L"LDA", false, false, { 0, 5 }, start) == DxuiTextView::FindResult::Wrapped,
                          L"backward from the lowercase one, ldax is skipped and the search goes round");
        Assert::AreEqual (1, start.row);
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

        Assert::IsTrue   (view.SelectMatch (L"sta", false, false, true) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (std::wstring (L"STA"), view.GetSelectionText(), L"the match is selected");
        Assert::AreEqual (0, view.GetTopLine(), L"row 5 is already in view");

        Assert::IsTrue   (view.SelectMatch (L"sta", false, false, true) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (std::wstring (L"STA"), view.GetSelectionText());
        Assert::AreEqual (80 - 10 + 1, view.GetTopLine(), L"row 80 is scrolled to the bottom of the view");

        Assert::IsTrue   (view.SelectMatch (L"sta", false, false, true) == DxuiTextView::FindResult::Wrapped);
        Assert::AreEqual (5, view.GetTopLine(), L"round to row 5, scrolled back up to it");

        Assert::IsTrue   (view.SelectMatch (L"sta", false, false, false) == DxuiTextView::FindResult::Wrapped);
        Assert::AreEqual (80 - 10 + 1, view.GetTopLine(), L"backward from row 5 goes round to row 80");

        Assert::IsTrue   (view.SelectMatch (L"LDA", false, false, true) == DxuiTextView::FindResult::NotFound);
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

        Assert::IsTrue   (view.SelectMatch (L"JUMP", true, false, true) == DxuiTextView::FindResult::Found);
        Assert::AreEqual (view.GetLineCount() - 4, view.GetTopLine(), L"the match's line shows, at the bottom of the view, not the row's first");
    }

    //  A view that follows its end stays at the bottom through a smaller
    //  layout, and keeps its place through new rows once scrolled up.
    TEST_METHOD (FollowEnd_StaysAtTheBottomThroughALayout)
    {
        DxuiTextView                    view;
        std::vector<DxuiTextView::Row>  rows;



        for (int i = 0; i < 100; i++)
        {
            rows.push_back (MakeRow ({ std::format (L"line {}", i) }));
        }

        view.SetFollowEnd (true);
        LayOut (view, 400, 12 + 16 * 10);
        view.SetRows (rows);
        view.SetTopLine (view.GetLineCount());

        LayOut (view, 400, 12 + 16 * 5);
        Assert::AreEqual (95, view.GetTopLine(), L"still at the bottom after shrinking");

        LayOut (view, 400, 0);
        LayOut (view, 400, 12 + 16 * 10);
        Assert::AreEqual (90, view.GetTopLine(), L"still at the bottom after a collapse");

        view.SetTopLine (20);
        rows.push_back (MakeRow ({ L"more" }));
        view.SetRows (rows);
        Assert::AreEqual (20, view.GetTopLine(), L"scrolled up, new rows keep its place");
    }
};
