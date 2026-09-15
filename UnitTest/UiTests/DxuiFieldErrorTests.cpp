#include "Pch.h"

#include "Widgets/DxuiFieldError.h"
#include "../Dxui/MockDxuiTextRenderer.h"
#include "../Dxui/MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiFieldErrorTests
//
//  The message under a field: how much room it takes as it wraps, which color
//  its mark's cross takes against the mark's fill, and how a validator holds
//  the confirming button back until every field passes.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiFieldErrorTests)
{
public:

    TEST_METHOD (Height_NoneWithoutAMessageAndMoreWhenItWraps)
    {
        DxuiFieldError        error;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;
        int                   wide   = 0;
        int                   narrow = 0;

        scaler.SetDpi (96);

        Assert::AreEqual (0, error.GetHeightPx (text, theme, scaler, 400), L"No message takes no room");

        error.SetMessage (L"Enter a volume number from 1 to 254, or leave it blank for 254.");
        wide   = error.GetHeightPx (text, theme, scaler, 4000);
        narrow = error.GetHeightPx (text, theme, scaler, 120);

        Assert::IsTrue (wide > 0,      L"A message takes room");
        Assert::IsTrue (narrow > wide, L"and more when it wraps onto further lines");
    }


    TEST_METHOD (MarkGlyph_StandsOutAgainstItsFill)
    {
        Assert::AreEqual (0xFF000000u, DxuiFieldError::GetMarkGlyphColor (0xFFFF99A4u), L"A pale red takes a black cross");
        Assert::AreEqual (0xFFFFFFFFu, DxuiFieldError::GetMarkGlyphColor (0xFFC42B1Cu), L"A deep red takes a white one");
    }


    TEST_METHOD (Validator_EnablesTheButtonOnlyWhenEveryFieldPasses)
    {
        DxuiFieldError      first;
        DxuiFieldError      second;
        DxuiButton          ok;
        DxuiFieldValidator  validator;
        std::wstring        firstMessage;
        std::wstring        secondMessage = L"Bad";

        validator.AddField ([&]() { return firstMessage; },  &first);
        validator.AddField ([&]() { return secondMessage; }, &second);
        validator.SetConfirmButton (&ok);

        Assert::IsFalse  (validator.Revalidate());
        Assert::IsFalse  (ok.IsEnabled(), L"A failing field holds the button back");
        Assert::IsFalse  (first.HasError());
        Assert::AreEqual (std::wstring (L"Bad"), second.GetMessage());

        secondMessage.clear();

        Assert::IsTrue   (validator.Revalidate());
        Assert::IsTrue   (ok.IsEnabled(), L"and every field passing lets it go");
        Assert::IsFalse  (second.HasError());
    }
};
