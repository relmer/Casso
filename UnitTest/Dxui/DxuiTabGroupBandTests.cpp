#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroupBandTests
//
//  A group's tabs lie in a band across the pane's full width, a step darker
//  than the pane, as Visual Studio draws them: along a document's top and
//  along a tool window's bottom. The band is rounded at the pane's outer
//  corners, so it is a rounded fill held to the strip.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiTabGroupBandTests
{
    struct Band
    {
        RECT      strip = {};
        bool      found = false;
        uint32_t  argb  = 0;
    };



    //  The band of a group of two panes, of the kind given: the rounded fill
    //  clipped to the strip, in the theme's band color.
    static Band PaintBand (DxuiTabGroup::Kind kind, const MockDxuiTheme & theme)
    {
        DxuiTabGroup          group;
        MockDxuiControl       a;
        MockDxuiControl       b;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        Band                  band;



        group.AddTab  (L"Memory 1", &a);
        group.AddTab  (L"Memory 2", &b);
        group.SetKind (kind);
        group.Layout  (RECT { 0, 0, 400, 300 }, scaler);
        group.Paint   (painter, text, theme);

        band.strip = group.GetStripRect();

        for (const RecordedPaintCall & call : painter.Calls())
        {
            bool  clippedToStrip = call.isClipped && call.clip.left == band.strip.left && call.clip.top == band.strip.top &&
                                   call.clip.right == band.strip.right && call.clip.bottom == band.strip.bottom;

            if (call.kind == RecordedPaintKind::FillRoundedRect && clippedToStrip && call.argb == theme.PaneBand())
            {
                band.found = true;
                band.argb  = call.argb;
            }
        }

        return band;
    }



    static int SumChannels (uint32_t argb)
    {
        return (int) ((argb >> 16) & 0xFF) + (int) ((argb >> 8) & 0xFF) + (int) (argb & 0xFF);
    }



    TEST_CLASS (DxuiTabGroupBandTests)
    {
    public:

        TEST_METHOD (TheBandIsFilledAcrossTheStrip)
        {
            MockDxuiTheme  theme;



            for (DxuiTabGroup::Kind kind : { DxuiTabGroup::Kind::Document, DxuiTabGroup::Kind::ToolWindow })
            {
                Band  band = PaintBand (kind, theme);

                Assert::IsTrue   (band.strip.bottom > band.strip.top, L"the group shows its tabs");
                Assert::IsTrue   (band.found,                         L"a rounded fill in the band color, clipped to the strip");
                Assert::AreEqual (400L, band.strip.right - band.strip.left, L"across the pane's full width");
            }
        }


        TEST_METHOD (TheBandIsDarkerThanTheContent)
        {
            MockDxuiTheme  theme;



            for (DxuiTabGroup::Kind kind : { DxuiTabGroup::Kind::Document, DxuiTabGroup::Kind::ToolWindow })
            {
                Band  band = PaintBand (kind, theme);

                Assert::IsTrue      (band.found, L"the band is painted");
                Assert::AreNotEqual (theme.ContentBackground(), band.argb, L"the band is a step off the pane");
                Assert::IsTrue      (SumChannels (band.argb) < SumChannels (theme.ContentBackground()), L"darker, not lighter");
            }
        }
    };
}
