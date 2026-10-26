#include "Pch.h"

#include "../Dxui/MockDxuiPainter.h"
#include "../Dxui/MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TextInputCaretTests
//
//  A focused field draws its caret only while its window is active: with
//  another app or another window in front, nothing blinks.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TextInputCaretTests)
{
public:

    //  An empty field draws no selection, so its only filled rect is the caret.
    static int CountFills (const MockDxuiTextRenderer & text)
    {
        int  fills = 0;

        for (const RecordedTextCall & call : text.Calls())
        {
            fills += (call.kind == RecordedTextKind::FillRect) ? 1 : 0;
        }

        return fills;
    }


    //  SetOnFocusLost's callback runs when the focus moves off the field, and
    //  not when it arrives.
    TEST_METHOD (LosingTheFocusCallsTheFocusLostCallback)
    {
        DxuiTextInput  input;
        int            calls = 0;



        input.SetOnFocusLost ([&calls] { calls++; });

        input.OnFocusChanged (true);
        Assert::AreEqual (0, calls);

        input.OnFocusChanged (false);
        Assert::AreEqual (1, calls);
    }



    //  A field given a new value by SetText, as one editor reused for each
    //  cell of a grid is, starts a new undo history: Undo does not bring
    //  back the text typed for the value before.
    TEST_METHOD (SetTextStartsANewUndoHistory)
    {
        DxuiTextInput  input;
        DxuiKeyEvent   ev;



        ev.kind = DxuiKeyEventKind::Char;
        input.SetFocused (true);

        for (wchar_t ch : std::wstring (L"foo"))
        {
            ev.vk = (WPARAM) ch;
            (void) input.OnKey (ev);
        }

        Assert::AreEqual (std::wstring (L"foo"), input.GetText());

        input.SetText (L"bar");
        (void) input.InvokeCommand (DxuiStandardCommand::Undo);

        Assert::AreEqual (std::wstring (L"bar"), input.GetText(), L"nothing of the earlier value to undo");
    }


    //  A drag in a toolbar's edit box ends at any release over the strip,
    //  the release the toolbar passes to every entry, though the pointer
    //  left the box and disarmed it; a later move selects nothing.
    TEST_METHOD (AToolbarEditBoxDragEndsAtAReleaseOutsideIt)
    {
        DxuiToolbarEditBox    box;
        MockDxuiTextRenderer  text;
        DxuiDpiScaler         scaler;
        DxuiKeyEvent          ev;
        RECT                  rc     = { 0, 0, 220, 24 };



        scaler.SetDpi (96);
        box.SetTextRenderer (&text);
        box.Layout     (rc, false, scaler);
        box.SetText    (L"hello world");
        box.SetFocused (true);

        (void) box.OnLButtonDown (rc.right - 10, 12);
        box.OnLButtonUp (rc.right + 50, 12);
        (void) box.OnMouseMove (rc.left + 5, 12);

        ev.kind = DxuiKeyEventKind::Char;
        ev.vk   = L'X';
        (void) box.OnKey (ev);

        Assert::AreEqual (std::wstring (L"hello worldX"), box.GetText(), L"no selection followed the pointer");
    }


    //  A log that drops its oldest rows moves the find highlights and the
    //  selection up with the text; rows replaced whole drop them.
    TEST_METHOD (ATextViewsFindStateMovesWithDroppedRows)
    {
        DxuiTextView                     view;
        std::vector<DxuiTextView::Row>   rows;
        int                              index = 0;
        int                              count = 0;



        for (const wchar_t * text : { L"aaa", L"bbb", L"find me", L"ccc" })
        {
            rows.push_back (DxuiTextView::Row { { text } });
        }

        view.SetRows (rows);
        (void) view.SelectMatch (L"find", false, false, false, true, index, count);

        Assert::AreEqual ((size_t) 1, view.GetFindHighlights().size());
        Assert::AreEqual (2, view.GetFindHighlights()[0].start.row);

        rows.erase (rows.begin());
        rows.push_back (DxuiTextView::Row { { L"ddd" } });
        view.SetRowsDroppingFront (rows, 1);

        Assert::AreEqual (1, view.GetFindHighlights().at (0).start.row, L"the highlight moved up with its row");
        Assert::AreEqual (std::wstring (L"find"), view.GetSelectionText(), L"and the selection with it");

        view.SetRows ({ DxuiTextView::Row { { L"other" } } });
        Assert::IsTrue (view.GetFindHighlights().empty(), L"new rows drop the old highlights");
    }


    TEST_METHOD (AFocusedFieldInAnActiveWindowDrawsItsCaret)
    {
        DxuiTextInput          input;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;

        input.SetRect    (RECT { 0, 0, 200, 24 });
        input.SetFocused (true);
        input.Paint      (painter, text);

        Assert::AreEqual (1, CountFills (text));
    }


    TEST_METHOD (AFocusedFieldInAnInactiveWindowDrawsNoCaret)
    {
        DxuiTextInput          input;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;

        input.SetRect         (RECT { 0, 0, 200, 24 });
        input.SetFocused      (true);
        input.SetWindowActive (false);
        input.Paint           (painter, text);

        Assert::AreEqual (0, CountFills (text));
    }


    TEST_METHOD (TheCaretReturnsWhenTheWindowIsActiveAgain)
    {
        DxuiTextInput          input;
        MockDxuiPainter        painter;
        MockDxuiTextRenderer   text;

        input.SetRect         (RECT { 0, 0, 200, 24 });
        input.SetFocused      (true);
        input.SetWindowActive (false);
        input.SetWindowActive (true);
        input.Paint           (painter, text);

        Assert::AreEqual (1, CountFills (text));
    }
};
