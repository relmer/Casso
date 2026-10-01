#include "Pch.h"

#include "Theme/DxuiColor.h"
#include "Widgets/DxuiHexView.h"
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
};
