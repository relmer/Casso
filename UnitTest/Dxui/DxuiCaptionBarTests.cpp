#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiCaptionBarTests
//
//  The title's tooltip: the full title when a paint had to cut it short, and
//  only over the title itself. The mock renderer's fixed glyph width decides
//  what fits, so the widths here are chosen to fit or not by a wide margin.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiCaptionBarTests)
{
public:

    struct Fixture
    {
        DxuiCaptionBar        caption;
        MockDxuiTextRenderer  text;
        MockDxuiPainter       painter;
        MockDxuiTheme         theme;
        DxuiDpiScaler         scaler;

        void  PaintAt (const std::wstring & title, int width)
        {
            caption.ConfigureButtons (DxuiCaptionBar::Buttons::MinMaxClose);
            caption.SetTitle (title);
            caption.Layout (RECT { 0, 0, width, DxuiCaptionBar::kCaptionHeightDip }, scaler);
            caption.Paint (painter, text, theme);
        }
    };


    TEST_METHOD (TitleThatFits_ShowsNoTip)
    {
        Fixture  f;
        RECT     anchor = {};

        f.PaintAt (L"Short", 800);

        Assert::IsNull (f.caption.GetTooltipAt (POINT { 30, 16 }, anchor));
    }


    TEST_METHOD (TitleCutShort_ShowsTheWholeTitle)
    {
        Fixture       f;
        RECT          anchor = {};
        std::wstring  title (200, L'x');

        f.PaintAt (title, 400);

        Assert::AreEqual (title.c_str(), f.caption.GetTooltipAt (POINT { 30, 16 }, anchor));
        Assert::IsTrue   (anchor.right > anchor.left, L"anchored on the title");
    }


    TEST_METHOD (TitleCutShort_NoTipOverTheButtons)
    {
        Fixture       f;
        RECT          anchor = {};
        std::wstring  title (200, L'x');

        f.PaintAt (title, 400);

        Assert::IsNull (f.caption.GetTooltipAt (POINT { 400 - 10, 16 }, anchor), L"the close button is not the title");
    }
};
