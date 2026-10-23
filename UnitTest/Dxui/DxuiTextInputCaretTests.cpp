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
