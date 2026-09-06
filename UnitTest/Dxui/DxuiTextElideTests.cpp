#include "Pch.h"

#include "Core/DxuiTextElide.h"
#include "MockDxuiTextRenderer.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextElideTests
//
//  Fitting text to a WIDTH rather than to a character count.
//
//  The mock renderer measures a flat 7 DIP per character, which is what makes
//  these deterministic -- the point of the class is that the real answer comes
//  from the renderer, and the tests exercise the search rather than any
//  particular font's metrics.
//
//  The two modes sacrifice opposite ends and both are covered, because which
//  end carries the meaning depends entirely on what the string is: the first
//  words identify a label, the last components identify a path.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTextElideTests)
{
public:

    static constexpr float  kGlyph = 7.0f;      // the mock's per-character width
    static constexpr float  kFont  = 13.0f;

    std::wstring Elide (MockDxuiTextRenderer & r, const wchar_t * s, float maxWidth, DxuiElide mode)
    {
        return DxuiTextElide::ToWidth (r, s, kFont, L"Segoe UI", maxWidth, mode);
    }


    TEST_METHOD (TextThatFitsIsUntouched)
    {
        MockDxuiTextRenderer   r;

        Assert::AreEqual (std::wstring (L"short"), Elide (r, L"short", 100.0f, DxuiElide::Tail));
        Assert::AreEqual (std::wstring (L"short"), Elide (r, L"short", 100.0f, DxuiElide::PathHead));
    }


    TEST_METHOD (ElideNoneNeverTrimsEvenWhenItOverflows)
    {
        MockDxuiTextRenderer   r;

        Assert::AreEqual (std::wstring (L"a string far too wide for this box"),
                          Elide (r, L"a string far too wide for this box", 20.0f, DxuiElide::None));
    }


    //
    //  Tail: keep the beginning, for labels.
    //

    TEST_METHOD (TailKeepsTheStartAndFitsTheBudget)
    {
        MockDxuiTextRenderer   r;
        std::wstring           out = Elide (r, L"An extremely long button label", 70.0f, DxuiElide::Tail);

        Assert::IsTrue (out.rfind (L"An", 0) == 0, L"the start survives");
        Assert::IsTrue (out.back() == L'\x2026', L"it ends with an ellipsis");
        Assert::IsTrue ((float) out.length() * kGlyph <= 70.0f, L"and it fits");
    }


    // A box too small for even one character plus the ellipsis still has to
    // say "there is more here"; an empty cell says the value is unset.
    TEST_METHOD (TailFallsBackToABareEllipsis)
    {
        MockDxuiTextRenderer   r;

        Assert::AreEqual (std::wstring (L"\x2026"), Elide (r, L"anything", 7.0f, DxuiElide::Tail));
    }


    //
    //  PathHead: keep the end, for paths.
    //

    TEST_METHOD (PathHeadKeepsTheLeafAndDropsTheHead)
    {
        MockDxuiTextRenderer   r;
        std::wstring           out = Elide (r, L"C:\\Users\\somebody\\Pictures\\Casso Screenshots",
                                            140.0f, DxuiElide::PathHead);

        Assert::IsTrue (out.rfind (L"\x2026", 0) == 0, L"it leads with an ellipsis");
        Assert::IsTrue (out.find (L"Casso Screenshots") != std::wstring::npos, L"the leaf survives");
        Assert::IsTrue (out.find (L"somebody") == std::wstring::npos, L"the head is dropped");
        Assert::IsTrue ((float) out.length() * kGlyph <= 140.0f, L"and it fits");
    }


    // A component sliced in half looks like a real folder and is not, so the
    // cut lands on a separator.
    TEST_METHOD (PathHeadCutsOnASeparator)
    {
        MockDxuiTextRenderer   r;
        std::wstring           out = Elide (r, L"C:\\Users\\somebody\\Pictures\\Casso Screenshots",
                                            140.0f, DxuiElide::PathHead);

        Assert::IsTrue (out.length() > 1);
        Assert::AreEqual (L'\\', out[1], L"the character after the ellipsis is a separator");
    }


    // Snapping to a separator only ever shortens, so a wider box must not
    // produce a longer result than the box allows.
    TEST_METHOD (PathHeadHonorsTheBudgetAtSeveralWidths)
    {
        MockDxuiTextRenderer   r;
        float                  widths[] = { 60.0f, 100.0f, 140.0f, 200.0f };
        size_t                 i        = 0;

        for (i = 0; i < std::size (widths); i++)
        {
            std::wstring   out = Elide (r, L"C:\\Users\\somebody\\Pictures\\Casso Screenshots",
                                        widths[i], DxuiElide::PathHead);

            Assert::IsTrue ((float) out.length() * kGlyph <= widths[i],
                            L"every width produces something that fits");
        }
    }


    // A path with no separator left to cut on still has to come back within
    // budget rather than overflow the row.
    TEST_METHOD (PathHeadWithNoSeparatorStillFits)
    {
        MockDxuiTextRenderer   r;
        std::wstring           out = Elide (r, L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 70.0f,
                                            DxuiElide::PathHead);

        Assert::IsTrue ((float) out.length() * kGlyph <= 70.0f);
        Assert::IsTrue (out.rfind (L"\x2026", 0) == 0);
    }


    //
    //  Degenerate inputs
    //

    TEST_METHOD (AnEmptyStringAndANonsenseWidthAreLeftAlone)
    {
        MockDxuiTextRenderer   r;

        Assert::AreEqual (std::wstring (L""),     Elide (r, L"", 100.0f, DxuiElide::Tail));
        Assert::AreEqual (std::wstring (L"text"), Elide (r, L"text", 0.0f,  DxuiElide::Tail));
        Assert::AreEqual (std::wstring (L"text"), Elide (r, L"text", -5.0f, DxuiElide::PathHead));
    }
};
