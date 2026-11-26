#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexViewSelectionContrastTests
//
//  The fill behind selected bytes reaches WCAG AA contrast for a non-text
//  mark against the view's background in every built-in theme, so a byte a
//  "Show ... in Memory" item selected stands out before the view has focus,
//  and the ink on it stays readable.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiHexViewSelectionContrastTests)
{
public:

    static constexpr float  kTextContrast = 4.5f;


    static void  AssertReadable (const CassoTheme & theme, const wchar_t * name)
    {
        uint32_t  fill = DxuiHexView::GetSelectionFill (theme);
        uint32_t  ink  = DxuiHexView::GetSelectionInk  (theme);

        Assert::IsTrue (DxuiColor::ComputeContrastRatio (fill, theme.ContentBackground()) >= DxuiHexView::kSelectionFillContrast, name);
        Assert::IsTrue (DxuiColor::ComputeContrastRatio (ink,  fill)                      >= kTextContrast,                       name);
    }


    TEST_METHOD (TheSelectionStandsOffTheBackgroundInEveryTheme)
    {
        AssertReadable (CassoTheme::MakeSkeuomorphic(),  L"skeuomorphic");
        AssertReadable (CassoTheme::MakeDarkModern(),    L"dark modern");
        AssertReadable (CassoTheme::MakeRetroTerminal(), L"retro terminal");
    }


    TEST_METHOD (AFillWithEnoughContrastIsLeftAsItIs)
    {
        Assert::AreEqual (0xFF808080u, DxuiColor::ComputeFillForContrast (0xFF808080u, 0xFF000000u, DxuiHexView::kSelectionFillContrast));
    }


    TEST_METHOD (AFaintFillOnALightBackgroundIsDarkened)
    {
        uint32_t  fill = DxuiColor::ComputeFillForContrast (0xFFF0F0F0u, 0xFFFFFFFFu, DxuiHexView::kSelectionFillContrast);

        Assert::IsTrue (DxuiColor::ComputeContrastRatio (fill, 0xFFFFFFFFu) >= DxuiHexView::kSelectionFillContrast);
        Assert::IsTrue (DxuiColor::ComputeRelativeLuminance (fill) < DxuiColor::ComputeRelativeLuminance (0xFFF0F0F0u));
    }


    //  The byte the keys act on is white or black on the accent, whichever
    //  reads: white on a bright cyan accent did not.
    TEST_METHOD (TheCaretsInkReadsOnTheAccentInEveryTheme)
    {
        for (const CassoTheme & theme : { CassoTheme::MakeSkeuomorphic(), CassoTheme::MakeDarkModern(), CassoTheme::MakeRetroTerminal() })
        {
            Assert::IsTrue (DxuiColor::ComputeContrastRatio (DxuiHexView::GetCaretInk (theme), theme.Accent()) >= kTextContrast);
        }

        Assert::AreEqual (0xFF000000u, DxuiColor::ChooseInkFor (0xFF00E5FFu, 0xFFFFFFFFu, 0xFF000000u), L"black on bright cyan");
    }


    TEST_METHOD (AnInkThatReadsIsLeftAndOneThatDoesNotIsMovedUntilItDoes)
    {
        uint32_t  moved = DxuiColor::ComputeInkForContrast (0xFFB8C0CAu, 0xFF3D6FB5u, kTextContrast);



        Assert::AreEqual (0xFFF0F0F0u, DxuiColor::ComputeInkForContrast (0xFFF0F0F0u, 0xFF101010u, kTextContrast), L"already reads");
        Assert::IsTrue   (DxuiColor::ComputeContrastRatio (moved, 0xFF3D6FB5u) >= kTextContrast, L"muted gray on a blue hover is lightened");
        Assert::AreEqual (0xFF808080u, DxuiColor::Composite (0x80FFFFFFu, 0xFF000000u) & 0xFFF0F0F0u, L"half white over black");
    }
};