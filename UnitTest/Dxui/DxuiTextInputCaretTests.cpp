#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TextInputCaretTests
//
//  The caret of a focused field is off while its window is not in front, as
//  an edit control's is when another program is active.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TextInputCaretTests)
{
public:

    TEST_METHOD (FocusedField_InAWindowNotInFront_ShowsNoCaret)
    {
        DxuiTextInput  input;
        HWND           hwnd = CreateWindowExW (0, L"STATIC", L"", WS_POPUP, 0, 0, 10, 10, nullptr, nullptr, GetModuleHandleW (nullptr), nullptr);

        Assert::IsNotNull (hwnd);
        Assert::AreNotEqual ((void *) hwnd, (void *) GetForegroundWindow(), L"a window never shown is never in front");

        input.SetHwnd    (hwnd);
        input.SetFocused (true);

        Assert::IsFalse (input.IsCaretOn());

        DestroyWindow (hwnd);
    }
};
