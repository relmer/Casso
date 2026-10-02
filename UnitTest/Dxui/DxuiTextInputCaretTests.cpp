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
