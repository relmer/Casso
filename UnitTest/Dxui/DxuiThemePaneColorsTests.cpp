#include "Pch.h"

#include "Ui/Chrome/CassoTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiThemePaneColorsTests
//
//  The colors around docked panes: the gap between them, the band behind
//  their tabs, and the drop targets a dragged pane shows. Each derives from
//  the theme's content fill and border unless the theme sets its own, and the
//  derivation reproduces Visual Studio's dark theme from its two colors.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiThemePaneColorsTests)
{
public:

    //  The gap is darker than the band, and the band darker than the content
    //  it sits beside, with the colors expected of the theme.
    static void  AssertGapAndBand (const DxuiTheme & theme, uint32_t gap, uint32_t band, const wchar_t * name)
    {
        float  gapLuminance     = DxuiColor::ComputeRelativeLuminance (theme.DockGap());
        float  bandLuminance    = DxuiColor::ComputeRelativeLuminance (theme.PaneBand());
        float  contentLuminance = DxuiColor::ComputeRelativeLuminance (theme.ContentBackground());



        Assert::AreEqual (gap,  theme.DockGap(),             name);
        Assert::AreEqual (band, theme.PaneBand(),            name);
        Assert::IsTrue   (gapLuminance  < bandLuminance,     name);
        Assert::IsTrue   (bandLuminance < contentLuminance,  name);
    }


    //  Visual Studio's dark theme: panes of #282828 edged in #454545, with
    //  #1C1C1C between them and #262626 behind their tabs.
    TEST_METHOD (DerivedColorsReproduceVisualStudio)
    {
        DxuiTheme  theme = {};



        theme.panelBg   = 0xFF282828;
        theme.contentBg = 0xFF282828;
        theme.panelEdge = 0xFF454545;

        Assert::AreEqual (0xFF1C1C1Cu, theme.DockGap());
        Assert::AreEqual (0xFF262626u, theme.PaneBand());
        Assert::AreEqual (0xFF333333u, theme.DockGuideBorder());
        Assert::AreEqual (0x99202020u, theme.DockGuideFill());
        Assert::AreEqual (0xFF363636u, theme.DockGuideButtonBorder());
        Assert::AreEqual (0xFF212121u, theme.DockGuideButtonFill());
    }


    TEST_METHOD (GapIsDarkerThanBandIsDarkerThanContentInEveryTheme)
    {
        DxuiDarkTheme   dark;
        DxuiLightTheme  light;



        AssertGapAndBand (CassoTheme::MakeSkeuomorphic(),  0xFF121721u, 0xFF18202Du, L"skeuomorphic");
        AssertGapAndBand (CassoTheme::MakeDarkModern(),    0xFF151619u, 0xFF1C1E22u, L"dark modern");
        AssertGapAndBand (CassoTheme::MakeRetroTerminal(), 0xFF091A0Cu, 0xFF0D2411u, L"retro terminal");
        AssertGapAndBand (dark,                            0xFF111111u, 0xFF171717u, L"system dark");
        AssertGapAndBand (light,                           0xFFEBEBEBu, 0xFFF3F3F3u, L"system light");
    }


    //  The system themes take Visual Studio's own drop-target colors, measured
    //  rather than derived. The light theme's button border is transparent:
    //  Visual Studio draws none there.
    TEST_METHOD (SystemThemesGiveTheMeasuredGuideColors)
    {
        DxuiDarkTheme   dark;
        DxuiLightTheme  light;



        Assert::AreEqual (0xFF333333u, dark.DockGuideBorder());
        Assert::AreEqual (0x99202020u, dark.DockGuideFill());
        Assert::AreEqual (0xFF363636u, dark.DockGuideButtonBorder());
        Assert::AreEqual (0xFF212121u, dark.DockGuideButtonFill());
        Assert::AreEqual (0xFFA0A0A0u, dark.DockGuideGlyph());
        Assert::AreEqual (0xFFA0A0A0u, dark.DockGuideArrow());

        Assert::AreEqual (0xFFCCCEDBu, light.DockGuideBorder());
        Assert::AreEqual (0xAAEAEAEEu, light.DockGuideFill());
        Assert::AreEqual (0x00F3F3F4u, light.DockGuideButtonBorder());
        Assert::AreEqual (0xFFF3F3F4u, light.DockGuideButtonFill());
        Assert::AreEqual (0xFF4893CEu, light.DockGuideGlyph());
        Assert::AreEqual (0xFF5D5D5Eu, light.DockGuideArrow());
    }


    TEST_METHOD (ATokenOverridesTheDerivedColor)
    {
        DxuiTheme  theme = DxuiTheme::Dark();



        Assert::AreEqual (theme.ForegroundMuted(), theme.DockGuideGlyph(), L"the glyph derives from the muted text");
        Assert::AreEqual (theme.DockGuideGlyph(),  theme.DockGuideArrow(), L"and the arrow from the glyph");

        theme.dockGap               = 0xFF010101;
        theme.paneBand              = 0xFF020202;
        theme.dockGuideBorder       = 0xFF030303;
        theme.dockGuideFill         = 0x80040404;
        theme.dockGuideButtonBorder = 0xFF050505;
        theme.dockGuideButtonFill   = 0xFF060606;
        theme.dockGuideGlyph        = 0xFF070707;

        Assert::AreEqual (0xFF010101u, theme.DockGap());
        Assert::AreEqual (0xFF020202u, theme.PaneBand());
        Assert::AreEqual (0xFF030303u, theme.DockGuideBorder());
        Assert::AreEqual (0x80040404u, theme.DockGuideFill());
        Assert::AreEqual (0xFF050505u, theme.DockGuideButtonBorder());
        Assert::AreEqual (0xFF060606u, theme.DockGuideButtonFill());
        Assert::AreEqual (0xFF070707u, theme.DockGuideGlyph());
        Assert::AreEqual (0xFF070707u, theme.DockGuideArrow(), L"an arrow with no token follows the glyph's");

        theme.dockGuideArrow = 0xFF080808;

        Assert::AreEqual (0xFF080808u, theme.DockGuideArrow());
    }
};
