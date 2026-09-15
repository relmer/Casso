#include "Pch.h"

#include "Widgets/DxuiTextInput.h"
#include "../Dxui/MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  TextInputWordSelectionTests
//
//  Double-click word selection, word-by-word drag, and the word rule itself.
//
//  The mock renderer measures every glyph 7 DIPs wide and the field pads its
//  text 6 DIPs from the left at 96 DPI, so character i spans
//  [6 + 7i, 13 + 7i). Clicks aim at the middle of a character.
//
//  Click timing comes from an injected clock and fixed double-click metrics,
//  so no test depends on the machine's mouse settings.
//
////////////////////////////////////////////////////////////////////////////////



TEST_CLASS (TextInputWordSelectionTests)
{
public:

    static constexpr int      kPadDip        = 6;
    static constexpr int      kGlyphDip      = 7;
    static constexpr int      kY             = 10;
    static constexpr UINT     kDoubleClickMs = 500;
    static constexpr int      kRectPx        = 4;
    static constexpr int64_t  kQuickMs       = 100;
    static constexpr int64_t  kSlowMs        = 600;
    static constexpr UINT     kDpi           = 96;

    // Index:  0         1
    //         012345678901234567
    static constexpr const wchar_t * kpszText = L"hello world_2, foo";

    static int GetCharX (size_t index)
    {
        return kPadDip + kGlyphDip * (int) index + kGlyphDip / 2;
    }

    struct Fixture
    {
        DxuiTextInput         input;
        MockDxuiTextRenderer  text;
        int64_t               now = 1000;

        Fixture()
        {
            RECT  rc = { 0, 0, 300, 30 };

            input.SetRect (rc);
            input.SetDpi (kDpi);
            input.SetText (kpszText);
            input.SetMaxLength (64);
            input.SetTextRenderer (&text);
            input.SetClock ([this]() { return now; });
            input.SetDoubleClickMetrics (kDoubleClickMs, kRectPx, kRectPx);
        }

        void Click (int x)
        {
            input.OnLButtonDown (x, kY);
            input.OnLButtonUp   (x, kY);
        }
    };

    static void AssertSpan (const std::wstring & s, size_t index, size_t expectStart, size_t expectEnd)
    {
        size_t  start = 99;
        size_t  end   = 99;

        DxuiTextInput::GetWordSpan (s, index, start, end);

        Assert::AreEqual (expectStart, start);
        Assert::AreEqual (expectEnd,   end);
    }

    static void AssertSelection (const DxuiTextInput & input, size_t expectStart, size_t expectEnd)
    {
        Assert::AreEqual (expectStart, input.GetSelectionStart());
        Assert::AreEqual (expectEnd,   input.GetSelectionEnd());
    }

    TEST_METHOD (WordSpan_LettersInMiddleOfWord)
    {
        AssertSpan (L"hello world", 2, 0, 5);
        AssertSpan (L"hello world", 8, 6, 11);
    }

    TEST_METHOD (WordSpan_DigitsAndUnderscoreJoinTheWord)
    {
        AssertSpan (kpszText, 11, 6, 13);
        AssertSpan (L"a1_b2 c", 0, 0, 5);
    }

    TEST_METHOD (WordSpan_WhitespaceSelectsTheRun)
    {
        AssertSpan (L"a   b", 2, 1, 4);
    }

    TEST_METHOD (WordSpan_PunctuationSelectsTheRunNotTheSpace)
    {
        AssertSpan (L"a..b", 1, 1, 3);
        AssertSpan (kpszText, 13, 13, 14);
    }

    TEST_METHOD (WordSpan_StartAndEndOfText)
    {
        AssertSpan (kpszText, 0,  0,  5);
        AssertSpan (kpszText, 17, 15, 18);
        AssertSpan (kpszText, 40, 15, 18);
    }

    TEST_METHOD (WordSpan_EmptyText)
    {
        AssertSpan (L"", 0, 0, 0);
        AssertSpan (L"", 5, 0, 0);
    }

    TEST_METHOD (DoubleClick_SelectsWordUnderPointer)
    {
        Fixture  f;

        f.Click (GetCharX (8));
        f.now += kQuickMs;
        f.Click (GetCharX (8));

        AssertSelection (f.input, 6, 13);
    }

    TEST_METHOD (DoubleClick_OnRightHalfOfLastLetterSelectsThatWord)
    {
        Fixture  f;
        int      x = kPadDip + kGlyphDip * 5 - 1;

        f.Click (x);
        f.now += kQuickMs;
        f.Click (x);

        AssertSelection (f.input, 0, 5);
    }

    TEST_METHOD (SlowSecondClick_IsANormalClick)
    {
        Fixture  f;

        f.Click (GetCharX (8));
        f.now += kSlowMs;
        f.Click (GetCharX (8));

        Assert::AreEqual (f.input.GetSelectionStart(), f.input.GetSelectionEnd());
    }

    TEST_METHOD (DistantSecondClick_IsANormalClick)
    {
        Fixture  f;

        f.Click (GetCharX (8));
        f.now += kQuickMs;
        f.Click (GetCharX (16));

        Assert::AreEqual (f.input.GetSelectionStart(), f.input.GetSelectionEnd());
    }

    TEST_METHOD (DoubleClickDrag_ExtendsByWordsForward)
    {
        Fixture  f;

        f.Click (GetCharX (1));
        f.now += kQuickMs;
        f.input.OnLButtonDown (GetCharX (1), kY);

        AssertSelection (f.input, 0, 5);

        f.input.OnMouseMove (GetCharX (8), kY);
        AssertSelection (f.input, 0, 13);

        f.input.OnMouseMove (GetCharX (16), kY);
        AssertSelection (f.input, 0, 18);

        // Back inside the anchor word: the anchor word stays selected.
        f.input.OnMouseMove (GetCharX (2), kY);
        AssertSelection (f.input, 0, 5);
    }

    TEST_METHOD (DoubleClickDrag_ExtendsByWordsBackward)
    {
        Fixture  f;

        f.Click (GetCharX (16));
        f.now += kQuickMs;
        f.input.OnLButtonDown (GetCharX (16), kY);

        AssertSelection (f.input, 15, 18);

        f.input.OnMouseMove (GetCharX (8), kY);
        AssertSelection (f.input, 6, 18);
        Assert::AreEqual ((size_t) 6, f.input.GetCaret());

        f.input.OnMouseMove (GetCharX (2), kY);
        AssertSelection (f.input, 0, 18);
    }

    TEST_METHOD (DragAfterSingleClick_StillSelectsByCharacter)
    {
        Fixture  f;

        f.input.OnLButtonDown (GetCharX (1) - kGlyphDip / 2, kY);
        f.input.OnMouseMove   (GetCharX (8) - kGlyphDip / 2, kY);

        AssertSelection (f.input, 1, 8);
    }

    TEST_METHOD (TripleClick_SelectsAll)
    {
        Fixture  f;

        f.Click (GetCharX (8));
        f.now += kQuickMs;
        f.Click (GetCharX (8));
        f.now += kQuickMs;
        f.Click (GetCharX (8));

        AssertSelection (f.input, 0, 18);
    }
};
