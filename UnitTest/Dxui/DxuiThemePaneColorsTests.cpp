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
//  derivation reproduces Visual Studio's dark and light themes from their
//  own colors.
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
    //  #1C1C1C between them and #262626 behind their tabs. A drop target's
    //  cross is #333333 over a translucent #202020, and the button under the
    //  pointer #1F1F1F inside #3D3D3D.
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
        Assert::AreEqual (0xFF3D3D3Du, theme.DockGuideButtonBorder());
        Assert::AreEqual (0xFF1F1F1Fu, theme.DockGuideButtonFill());
    }


    //  The Skeuomorphic theme's button under the pointer: #141A25 inside
    //  #2C3847, on a cross of #232D3C over a translucent #141B26.
    TEST_METHOD (DerivedGuideColorsForTheSkeuomorphicTheme)
    {
        CassoTheme  theme = CassoTheme::MakeSkeuomorphic();



        Assert::AreEqual (0xFF232D3Cu, theme.DockGuideBorder());
        Assert::AreEqual (0x99141B26u, theme.DockGuideFill());
        Assert::AreEqual (0xFF2C3847u, theme.DockGuideButtonBorder());
        Assert::AreEqual (0xFF141A25u, theme.DockGuideButtonFill());
        Assert::AreEqual (theme.Foreground(), theme.DockGuideGlyph(), L"the picture is the body text");
    }


    TEST_METHOD (GapIsDarkerThanBandIsDarkerThanContentInEveryTheme)
    {
        DxuiDarkTheme   dark;
        DxuiLightTheme  light;



        AssertGapAndBand (CassoTheme::MakeSkeuomorphic(),  0xFF121721u, 0xFF18202Du, L"skeuomorphic");
        AssertGapAndBand (CassoTheme::MakeDarkModern(),    0xFF151619u, 0xFF1C1E22u, L"dark modern");
        AssertGapAndBand (CassoTheme::MakeRetroTerminal(), 0xFF091A0Cu, 0xFF0D2411u, L"retro terminal");
        AssertGapAndBand (dark,                            0xFF111111u, 0xFF171717u, L"system dark");
        AssertGapAndBand (light,                           0xFFEFEFEFu, 0xFFF8F8F8u, L"system light");
    }


    //  Visual Studio's light theme: panes of #F9F9F9, with #EEEEEE between
    //  them and #F7F7F7 behind their tabs. The gap comes from the content
    //  alone, so a darker panel color leaves it where it is.
    TEST_METHOD (LightFactorsReproduceVisualStudio)
    {
        DxuiTheme  theme = {};



        theme.panelBg   = 0xFFFBFBFB;
        theme.contentBg = 0xFFF9F9F9;
        theme.panelEdge = 0xFFADADAD;

        Assert::AreEqual (0xFFEEEEEEu, theme.DockGap(),  L"the gap");
        Assert::AreEqual (0xFFF7F7F7u, theme.PaneBand(), L"the band");

        theme.panelBg = 0xFFE0E0E0;

        Assert::AreEqual (0xFFEEEEEEu, theme.DockGap(),  L"a darker panel does not pull the gap down");
    }


    //  A text view's background is the content color unless a theme sets one,
    //  and the focus accent is derived unless a theme sets one.
    TEST_METHOD (TextViewAndFocusAccentTokensOverrideTheDefaults)
    {
        DxuiTheme  theme   = DxuiTheme::Light();
        uint32_t   derived = theme.FocusAccent();



        Assert::AreEqual (theme.ContentBackground(), theme.TextViewBackground(), L"the content color by default");
        Assert::AreEqual (DxuiColor::ComputeFocusAccent (theme.Accent(), theme.Background()), derived, L"the derived accent by default");

        theme.textViewBg  = 0xFFFFFFFF;
        theme.focusAccent = 0xFF5649B0;

        Assert::AreEqual (0xFFFFFFFFu, theme.TextViewBackground());
        Assert::AreEqual (0xFF5649B0u, theme.FocusAccent());
        Assert::AreNotEqual (theme.TextViewBackground(), theme.ContentBackground(), L"the content color stays its own");
    }


    //  The system themes take Visual Studio's own drop-target colors, measured
    //  rather than derived: the button under the pointer at full strength,
    //  which every other button shows at 70%, and the shade over where the
    //  pane would go. The light theme's button border is transparent: Visual
    //  Studio draws none there.
    TEST_METHOD (SystemThemesGiveTheMeasuredGuideColors)
    {
        DxuiDarkTheme   dark;
        DxuiLightTheme  light;



        Assert::AreEqual (0xFF333333u, dark.DockGuideBorder());
        Assert::AreEqual (0x99202020u, dark.DockGuideFill());
        Assert::AreEqual (0xFF3D3D3Du, dark.DockGuideButtonBorder());
        Assert::AreEqual (0xFF1F1F1Fu, dark.DockGuideButtonFill());
        Assert::AreEqual (0xFFD6D6D6u, dark.DockGuideGlyph());
        Assert::AreEqual (0xFFD6D6D6u, dark.DockGuideArrow());
        Assert::AreEqual (0x1E3298E6u, dark.DockPreview());

        Assert::AreEqual (0xFFCCCEDBu, light.DockGuideBorder());
        Assert::AreEqual (0x99E8E8EDu, light.DockGuideFill());
        Assert::AreEqual (0x00F5F5F5u, light.DockGuideButtonBorder());
        Assert::AreEqual (0xFFF5F5F5u, light.DockGuideButtonFill());
        Assert::AreEqual (0xFF006CBEu, light.DockGuideGlyph());
        Assert::AreEqual (0xFF1E1E1Eu, light.DockGuideArrow());
        Assert::AreEqual (0x1E0078D4u, light.DockPreview());
    }


    //  A theme with no shade of its own shades in its accent, as faint as
    //  Visual Studio's: alpha 0x1E.
    TEST_METHOD (TheShadeIsTheAccentFaint)
    {
        CassoTheme  theme = CassoTheme::MakeSkeuomorphic();



        Assert::AreEqual ((theme.Accent() & 0x00FFFFFFu) | 0x1E000000u, theme.DockPreview());
    }


    TEST_METHOD (ATokenOverridesTheDerivedColor)
    {
        DxuiTheme  theme = DxuiTheme::Dark();



        Assert::AreEqual (theme.Foreground(),     theme.DockGuideGlyph(), L"the glyph derives from the body text");
        Assert::AreEqual (theme.DockGuideGlyph(), theme.DockGuideArrow(), L"and the arrow from the glyph");

        theme.dockGap               = 0xFF010101;
        theme.paneBand              = 0xFF020202;
        theme.dockGuideBorder       = 0xFF030303;
        theme.dockGuideFill         = 0x80040404;
        theme.dockGuideButtonBorder = 0xFF050505;
        theme.dockGuideButtonFill   = 0xFF060606;
        theme.dockGuideGlyph        = 0xFF070707;
        theme.dockPreview           = 0x1E090909;

        Assert::AreEqual (0xFF010101u, theme.DockGap());
        Assert::AreEqual (0xFF020202u, theme.PaneBand());
        Assert::AreEqual (0xFF030303u, theme.DockGuideBorder());
        Assert::AreEqual (0x80040404u, theme.DockGuideFill());
        Assert::AreEqual (0xFF050505u, theme.DockGuideButtonBorder());
        Assert::AreEqual (0xFF060606u, theme.DockGuideButtonFill());
        Assert::AreEqual (0xFF070707u, theme.DockGuideGlyph());
        Assert::AreEqual (0xFF070707u, theme.DockGuideArrow(), L"an arrow with no token follows the glyph's");
        Assert::AreEqual (0x1E090909u, theme.DockPreview());

        theme.dockGuideArrow = 0xFF080808;

        Assert::AreEqual (0xFF080808u, theme.DockGuideArrow());
    }
};
