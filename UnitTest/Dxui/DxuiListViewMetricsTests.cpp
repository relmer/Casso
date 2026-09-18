#include "Pch.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewMetricsTests
//
//  Row height, cell padding and text size belong to the instance, so a dense
//  list (a debugger's register or breakpoint pane) and a roomy one (a file
//  browser) can sit in the same process without one resizing the other.
//
//  The defaults are the assertion that matters most: every list that never
//  calls a setter must measure exactly as it did before the setters existed.
//
//  The mock text renderer measures every string as seven DIPs a character
//  whatever its size, so padding shows up in the measured width alone, and
//  the text size shows up in what the paint pass hands the renderer.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewMetricsTests)
{
public:

    static constexpr int  kCharWidthDip = 7;   // the mock's measure



    struct Fixture
    {
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiListView          list;

        void  Build (const wchar_t * cellText)
        {
            DxuiDpiScaler                                 scaler;
            std::vector<DxuiListView::Column>             cols;
            std::vector<std::vector<DxuiListView::Cell>>  data;

            scaler.SetDpi (96);

            cols.push_back (DxuiListView::Column { L"", 0 });
            data.push_back ({ DxuiListView::Cell { cellText, false } });

            list.SetColumns    (std::move (cols));
            list.SetShowHeader (false);
            list.SetRows       (std::move (data));
            list.Layout        (RECT { 0, 0, 400, 300 }, scaler);
        }

        int  MeasuredWidth()
        {
            list.MeasureColumnsPx (text);
            return list.GetTotalMeasuredWidthPx();
        }

        float  PaintedCellSize (const wchar_t * cellText)
        {
            float  size = 0.0f;

            text.Reset();
            list.Paint (painter, text, theme);

            for (const RecordedTextCall & call : text.Calls())
            {
                if (call.kind == RecordedTextKind::DrawString && call.text == cellText)
                {
                    size = call.fontSizeDip;
                }
            }

            return size;
        }
    };



    TEST_METHOD (Defaults_MeasureAsBefore)
    {
        Fixture  f;

        f.Build (L"ABCD");

        Assert::AreEqual (30,    f.list.GetRowHeightDip());
        Assert::AreEqual (12,    f.list.GetCellPadLeftDip());
        Assert::AreEqual (16,    f.list.GetCellPadRightDip());
        Assert::AreEqual (13.0f, f.list.GetFontSizeDip());
        Assert::AreEqual (4 * kCharWidthDip + 12 + 16, f.MeasuredWidth());
    }


    TEST_METHOD (CellPadding_ChangesTheMeasuredWidth)
    {
        Fixture  f;

        f.Build (L"ABCD");
        f.list.SetCellPaddingDip (4, 4);

        Assert::AreEqual (4 * kCharWidthDip + 4 + 4, f.MeasuredWidth());
    }


    TEST_METHOD (CellPadding_BelongsToOneInstance)
    {
        Fixture  dense;
        Fixture  roomy;

        dense.Build (L"ABCD");
        roomy.Build (L"ABCD");
        dense.list.SetCellPaddingDip (2, 2);

        Assert::AreEqual (4 * kCharWidthDip + 2 + 2,   dense.MeasuredWidth());
        Assert::AreEqual (4 * kCharWidthDip + 12 + 16, roomy.MeasuredWidth());
    }


    TEST_METHOD (CellPadding_NegativeFallsBackToTheDefault)
    {
        Fixture  f;

        f.Build (L"ABCD");
        f.list.SetCellPaddingDip (-1, -5);

        Assert::AreEqual (12, f.list.GetCellPadLeftDip());
        Assert::AreEqual (16, f.list.GetCellPadRightDip());
    }


    TEST_METHOD (FontSize_ReachesThePaintedText)
    {
        Fixture  f;

        f.Build (L"ABCD");

        Assert::AreEqual (13.0f, f.PaintedCellSize (L"ABCD"));

        f.list.SetFontSizeDip (11.0f);

        Assert::AreEqual (11.0f, f.PaintedCellSize (L"ABCD"));
    }


    TEST_METHOD (FontSize_NonPositiveFallsBackToTheDefault)
    {
        Fixture  f;

        f.Build (L"ABCD");
        f.list.SetFontSizeDip (0.0f);

        Assert::AreEqual (13.0f, f.list.GetFontSizeDip());
    }


    TEST_METHOD (HeaderHeight_BelongsToOneInstance)
    {
        Fixture  dense;
        Fixture  roomy;

        dense.Build (L"A");
        roomy.Build (L"A");
        dense.list.SetShowHeader      (true);
        roomy.list.SetShowHeader      (true);
        dense.list.SetHeaderHeightDip (22);

        Assert::AreEqual (22, dense.list.GetHeaderHeightPx(), L"at 96 DPI a DIP is a pixel");
        Assert::AreEqual (32, roomy.list.GetHeaderHeightPx());

        dense.list.SetHeaderHeightDip (0);
        Assert::AreEqual (32, dense.list.GetHeaderHeightPx(), L"a non-positive height restores the default");
    }


    //  A pane whose rows change every refresh has to fit them each time; a file
    //  list keeps the widths it was first given, which is what saves it
    //  re-measuring thousands of names on every refill.
    static int  WidthAfterRefill (bool refit)
    {
        Fixture  f;

        f.Build (L"A");
        f.list.SetPreciseAutoFit  (true);
        f.list.SetRefitOnSetRows  (refit);
        (void) f.PaintedCellSize  (L"A");

        f.list.SetRows ({ { DxuiListView::Cell { L"ABCDEFGH", false } } });
        (void) f.PaintedCellSize  (L"ABCDEFGH");

        return f.list.GetTotalMeasuredWidthPx();
    }


    TEST_METHOD (Refit_WiderRowsWidenTheColumn)
    {
        Assert::AreEqual (8 * kCharWidthDip + 12 + 16, WidthAfterRefill (true));
    }


    TEST_METHOD (Refit_OffByDefault_TheColumnKeepsItsWidth)
    {
        Assert::AreEqual (1 * kCharWidthDip + 12 + 16, WidthAfterRefill (false));
    }


    TEST_METHOD (RowHeight_BelongsToOneInstance)
    {
        Fixture  dense;
        Fixture  roomy;

        dense.Build (L"A");
        roomy.Build (L"A");
        dense.list.SetRowHeightDip (17);

        Assert::AreEqual (17, dense.list.GetRowHeightDip());
        Assert::AreEqual (30, roomy.list.GetRowHeightDip());
    }
};
