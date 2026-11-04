#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TextInputRejectedCharsTests
//
//  Characters a field doesn't take don't appear in its text, whether typed
//  one at a time or pasted in a block. A paste is given its
//  text here, as the clipboard would.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (TextInputRejectedCharsTests)
{
public:

    struct Fixture
    {
        DxuiTextInput  input;
        int            refusals = 0;

        Fixture()
        {
            input.SetRejectedChars (L"|\t\r\n");
            input.SetOnRejected    ([this]() { refusals++; });
            input.SetFocused       (true);
        }

        void  Type (const std::wstring & text)
        {
            for (wchar_t c : text)
            {
                (void) input.OnChar (c);
            }
        }
    };


    TEST_METHOD (TypedBar_IsRefused_AndReported)
    {
        Fixture  f;

        f.Type (L"A|B");

        Assert::AreEqual (std::wstring (L"AB"), f.input.GetText());
        Assert::AreEqual (1, f.refusals);
    }


    TEST_METHOD (OrdinaryText_IsTaken_WithNoReport)
    {
        Fixture  f;

        f.Type (L"Hard Hat Mack");

        Assert::AreEqual (std::wstring (L"Hard Hat Mack"), f.input.GetText());
        Assert::AreEqual (0, f.refusals);
    }


    TEST_METHOD (ARefusedCharacter_LeavesTheSelectionInPlace)
    {
        Fixture  f;

        f.Type (L"Choplifter");
        f.input.SelectAll();
        f.Type (L"|");

        Assert::AreEqual (std::wstring (L"Choplifter"), f.input.GetText(), L"nothing typed, so nothing replaced");
    }


    TEST_METHOD (PastedBlock_HasNoBarTabOrLineBreak)
    {
        Fixture  f;

        f.input.PasteText (L"Dane Bigham|Gene Portwood\r\nLauren\tElliott\n");

        Assert::AreEqual (std::wstring (L"Dane BighamGene Portwood  Lauren Elliott "), f.input.GetText());
        Assert::AreEqual (1, f.refusals, L"one report for the bar; line breaks and tabs become spaces");
    }


    TEST_METHOD (NoRejectedChars_TakesABar)
    {
        DxuiTextInput  input;

        input.SetFocused (true);
        (void) input.OnChar (L'|');

        Assert::AreEqual (std::wstring (L"|"), input.GetText());
    }
};
