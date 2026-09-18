#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiStatusBarTests
//
//  Field layout with and without a stretch field, text replacement, the band
//  height matching the menu bar strip, and one drawn string per non-empty
//  field.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiStatusBarTests)
{
public:

    static void  LayOut (DxuiStatusBar & bar, UINT dpi = 96)
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (dpi);
        bar.Layout (RECT { 0, 0, 600, 24 }, scaler);
    }


    TEST_METHOD (StretchField_TakesTheRemainder)
    {
        DxuiStatusBar  bar;

        bar.SetFields ({ { L"12 items", 0, true }, { L"DOS 3.3", 100, false }, { L"140 KB", 80, false } });
        LayOut (bar);

        Assert::AreEqual (0L,   bar.GetFieldRect (0).left);
        Assert::AreEqual (420L, bar.GetFieldRect (0).right);
        Assert::AreEqual (520L, bar.GetFieldRect (1).right);
        Assert::AreEqual (600L, bar.GetFieldRect (2).right);
    }


    TEST_METHOD (NoStretchField_LeavesTheRestEmpty)
    {
        DxuiStatusBar  bar;

        bar.SetFields ({ { L"a", 100, false }, { L"b", 50, false } });
        LayOut (bar);

        Assert::AreEqual (150L, bar.GetFieldRect (1).right);
    }


    TEST_METHOD (HighDpi_ScalesFixedWidths)
    {
        DxuiStatusBar  bar;
        DxuiDpiScaler  scaler;

        scaler.SetDpi (192);
        bar.SetFields ({ { L"a", 0, true }, { L"b", 100, false } });
        bar.Layout (RECT { 0, 0, 1000, 48 }, scaler);

        Assert::AreEqual (800L, bar.GetFieldRect (0).right);
    }


    TEST_METHOD (SetText_ReplacesOneFieldAndIgnoresOutOfRange)
    {
        DxuiStatusBar  bar;

        bar.SetFields ({ { L"a", 0, true }, { L"b", 50, false } });
        bar.SetText (1, L"changed");
        bar.SetText (7, L"ignored");

        Assert::AreEqual (std::wstring (L"changed"), bar.GetField (1).text);
        Assert::AreEqual (std::wstring (L"a, changed"), bar.GetAccessibleName());
    }


    TEST_METHOD (BandHeight_MatchesMenuBarStrip)
    {
        Assert::AreEqual (DxuiMenuBar::GetStripHeightPx (96), DxuiStatusBar::GetBandDp());
    }


    TEST_METHOD (Paint_DrawsOneStringPerNonEmptyField)
    {
        DxuiStatusBar         bar;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        int                   strings = 0;

        bar.SetFields ({ { L"12 items", 0, true }, { L"", 100, false }, { L"140 KB", 80, false } });
        LayOut (bar);

        bar.Paint (painter, text, theme);

        for (const RecordedTextCall & call : text.Calls())
        {
            if (call.kind == RecordedTextKind::DrawString)
            {
                strings++;
            }
        }

        Assert::AreEqual (2, strings);
    }
};
