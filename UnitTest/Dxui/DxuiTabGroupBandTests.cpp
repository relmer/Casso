#include "Pch.h"

#include "Widgets/DxuiTabGroup.h"
#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabGroupBandTests
//
//  A tool window's bottom tabs lie in a band a shade off the window's own
//  surface, as Visual Studio draws them, so the band reads apart from the
//  pane above it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiTabGroupBandTests)
{
public:

    TEST_METHOD (TheBottomBandIsAShadeOffTheBackground)
    {
        DxuiTabGroup          group;
        MockDxuiControl       a;
        MockDxuiControl       b;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        RECT                  strip  = {};
        bool                  filled = false;
        uint32_t              band   = 0;

        group.AddTab  (L"Memory 1", &a);
        group.AddTab  (L"Memory 2", &b);
        group.SetKind (DxuiTabGroup::Kind::ToolWindow);
        group.Layout  (RECT { 0, 0, 400, 300 }, scaler);
        group.Paint   (painter, text, theme);

        strip = group.GetStripRect();

        for (const RecordedPaintCall & call : painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRect && call.x == (float) strip.left && call.y == (float) strip.top &&
                call.width == (float) (strip.right - strip.left) && call.height == (float) (strip.bottom - strip.top))
            {
                filled = true;
                band   = call.argb;
            }
        }

        Assert::IsTrue      (filled, L"the band is filled across the strip");
        Assert::AreNotEqual (theme.Background(), band, L"the band is a shade off the background");
    }
};
