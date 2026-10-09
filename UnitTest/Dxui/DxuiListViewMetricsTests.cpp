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


    //  A list that fills a pane, laid out a little in from the window's
    //  corner, with a debugger pane's padding, one selected row and a fill
    //  behind its first cell.
    static constexpr int       kPaneLeftPx  = 30;
    static constexpr int       kPaneTopPx   = 10;
    static constexpr int       kPanePadDip  = 4;
    static constexpr uint32_t  kCellFill    = 0xFF123456;

    //  106 DPI is where the inset in pixels is not the sum of its parts
    //  converted one at a time.
    static constexpr int       kDpis[]      = { 96, 106, 120, 144, 168 };


    static void  PaintPaneList (DxuiListView & list, MockDxuiPainter & painter, MockDxuiTextRenderer & text, int dpi, bool paneInset)
    {
        DxuiDpiScaler                    scaler;
        MockDxuiTheme                    theme;
        std::vector<DxuiListView::Cell>  cells (2);



        scaler.SetDpi (dpi);

        cells[0].text       = L"ABCD";
        cells[0].background = kCellFill;
        cells[1].text       = L"EF";

        list.SetColumns             ({ DxuiListView::Column { L"Name", 0 }, DxuiListView::Column { L"Value", 0 } });
        list.SetShowHeader          (true);
        list.SetCellPaddingDip      (kPanePadDip, kPanePadDip);
        list.SetPaneTextInset       (paneInset);
        list.SetAlwaysShowSelection (true);
        list.SetRows                ({ cells });
        list.SetSelectedRow         (0);
        list.Layout                 (RECT { kPaneLeftPx, kPaneTopPx, kPaneLeftPx + 400, kPaneTopPx + 300 }, scaler);
        list.Paint                  (painter, text, theme);
    }


    static const RecordedTextCall * FindText (const MockDxuiTextRenderer & text, const wchar_t * string)
    {
        auto  found = std::ranges::find_if (text.Calls(), [string] (const RecordedTextCall & call)
        {
            return call.kind == RecordedTextKind::DrawString && call.text == string;
        });



        return (found != text.Calls().end()) ? &*found : nullptr;
    }


    static const RecordedPaintCall * FindFill (const MockDxuiPainter & painter, RecordedPaintKind kind, uint32_t argb)
    {
        auto  found = std::ranges::find_if (painter.Calls(), [kind, argb] (const RecordedPaintCall & call)
        {
            return call.kind == kind && call.argb == argb;
        });



        return (found != painter.Calls().end()) ? &*found : nullptr;
    }


    //  The first column's text, in the header and in the rows, starts at the
    //  pane's text inset, where the pane's title does; the selected row and
    //  the first cell's fill still run from the list's left.
    TEST_METHOD (PaneTextInsetMovesColumnZeroNotTheRowFill)
    {
        MockDxuiTheme  theme;



        for (int dpi : kDpis)
        {
            DxuiListView               plain;
            DxuiListView               inset;
            MockDxuiPainter            plainPainter;
            MockDxuiPainter            insetPainter;
            MockDxuiTextRenderer       plainText;
            MockDxuiTextRenderer       insetText;
            DxuiDpiScaler              scaler;
            int                        textX      = 0;
            int                        lead       = 0;
            RECT                       cellRect   = {};
            std::wstring               at         = std::format (L"at {} DPI", dpi);
            const RecordedTextCall   * header     = nullptr;
            const RecordedTextCall   * cell       = nullptr;
            const RecordedPaintCall  * selected   = nullptr;
            const RecordedPaintCall  * plainFill  = nullptr;
            const RecordedPaintCall  * insetFill  = nullptr;



            scaler.SetDpi (dpi);
            textX = DxuiPaneMetrics::GetContentTextInsetPx (scaler);
            lead  = textX - scaler.ToPx (kPanePadDip);

            PaintPaneList (plain, plainPainter, plainText, dpi, false);
            PaintPaneList (inset, insetPainter, insetText, dpi, true);

            header    = FindText (insetText, L"Name");
            cell      = FindText (insetText, L"ABCD");
            selected  = FindFill (insetPainter, RecordedPaintKind::FillRoundedRect, theme.ContentSelection());
            plainFill = FindFill (plainPainter, RecordedPaintKind::FillRect, kCellFill);
            insetFill = FindFill (insetPainter, RecordedPaintKind::FillRect, kCellFill);

            Assert::IsNotNull (header,    at.c_str());
            Assert::IsNotNull (cell,      at.c_str());
            Assert::IsNotNull (selected,  at.c_str());
            Assert::IsNotNull (plainFill, at.c_str());
            Assert::IsNotNull (insetFill, at.c_str());

            Assert::AreEqual ((float) (kPaneLeftPx + textX), header->x,   (L"the heading starts at the inset " + at).c_str());
            Assert::AreEqual ((float) (kPaneLeftPx + textX), cell->x,     (L"the first cell's text starts at the inset " + at).c_str());
            Assert::AreEqual ((float) kPaneLeftPx,           selected->x, (L"the selected row still starts at the list's left " + at).c_str());
            Assert::AreEqual ((float) kPaneLeftPx,           insetFill->x, (L"the first cell's fill still starts at the list's left " + at).c_str());
            Assert::AreEqual (plainFill->width + (float) lead, insetFill->width, (L"and runs on to the cell's end " + at).c_str());

            Assert::IsTrue   (inset.GetCellTextRectPx (0, 0, cellRect), at.c_str());
            Assert::AreEqual ((LONG) textX, cellRect.left, (L"an edit over the cell lines up with its text " + at).c_str());

            Assert::AreEqual (plain.GetContentWidthPx() + lead,       inset.GetContentWidthPx(),       (L"the content is wider by the lead " + at).c_str());
            Assert::AreEqual (plain.GetTotalMeasuredWidthPx() + lead, inset.GetTotalMeasuredWidthPx(), (L"and so is the measured width " + at).c_str());
        }
    }


    TEST_METHOD (PaneTextInset_OffByDefault)
    {
        DxuiListView  list;

        Assert::IsFalse (list.HasPaneTextInset());
    }


    //  Paints a code list's one row: a breakpoint's icon in a 17-DIP glyph
    //  column, then an address, with or without a glyph margin.
    static void  PaintCodeRow (MockDxuiTextRenderer & text, int dpi, float glyphCenterDip)
    {
        DxuiDpiScaler                    scaler;
        DxuiListView                     list;
        MockDxuiPainter                  painter;
        MockDxuiTheme                    theme;
        auto                             image = std::make_shared<DxuiIconImage>();
        std::vector<DxuiListView::Cell>  cells (2);



        scaler.SetDpi (dpi);

        image->width  = 4;
        image->height = 4;
        image->bgraPremul.assign (16, 0xFF0000FFu);

        cells[0].icon = image;
        cells[1].text = L"0300";

        list.SetColumns        ({ DxuiListView::Column { L"", 17 }, DxuiListView::Column { L"Address", 0 } });
        list.SetShowHeader     (false);
        list.SetCellPaddingDip (kPanePadDip, kPanePadDip);
        list.SetGlyphCenterDip (glyphCenterDip);
        list.SetRows           ({ cells });
        list.Layout            (RECT { kPaneLeftPx, kPaneTopPx, kPaneLeftPx + 400, kPaneTopPx + 300 }, scaler);
        list.Paint             (painter, text, theme);
    }


    //  A glyph margin centers column 0's icon on a set line from the list's
    //  left, as Visual Studio centers a breakpoint 8.4 DIP in: 8.4, 10.5 and
    //  12.6 pixels at 100, 125 and 150%. Without one the icon follows the
    //  cell's padding, as before.
    TEST_METHOD (AGlyphMarginCentersColumnZerosIconOnItsLine)
    {
        constexpr float  kCenterDip   = 8.4f;
        constexpr int    kIconDip     = 16;    // the list's icon size
        constexpr int    kGlyphDpis[] = { 96, 120, 144 };



        for (int dpi : kGlyphDpis)
        {
            DxuiDpiScaler         scaler;
            MockDxuiTextRenderer  margin;
            MockDxuiTextRenderer  plain;
            float                 center = 0.0f;
            std::wstring          at     = std::format (L"at {} DPI", dpi);



            scaler.SetDpi (dpi);

            PaintCodeRow (margin, dpi, kCenterDip);
            PaintCodeRow (plain,  dpi, 0.0f);

            Assert::AreEqual ((size_t) 1, margin.IconCalls().size(), at.c_str());
            Assert::AreEqual ((size_t) 1, plain.IconCalls().size(),  at.c_str());

            center = margin.IconCalls()[0].x + margin.IconCalls()[0].width * 0.5f;

            Assert::AreEqual ((float) kPaneLeftPx + scaler.ToPxf (kCenterDip), center, 0.001f, (L"centered on the line " + at).c_str());
            Assert::AreEqual (scaler.ToPxf ((float) kIconDip), margin.IconCalls()[0].width, (L"at the list's icon size " + at).c_str());
            Assert::AreEqual ((float) (kPaneLeftPx + scaler.ToPx (kPanePadDip)), plain.IconCalls()[0].x, (L"after the padding without a margin " + at).c_str());
        }
    }


    //  A list of code takes a text view's background for its rows, where any
    //  other list takes the content color.
    TEST_METHOD (ATextViewSurfaceFillsTheRowsInTheTextViewColor)
    {
        DxuiTheme             theme = DxuiTheme::Light();
        DxuiDpiScaler         scaler;
        MockDxuiPainter       codePainter;
        MockDxuiPainter       listPainter;
        MockDxuiTextRenderer  text;
        DxuiListView          code;
        DxuiListView          list;



        theme.contentBg  = 0xFFF9F9F9;
        theme.textViewBg = 0xFFFFFFFF;

        for (DxuiListView * each : { &code, &list })
        {
            each->SetColumns ({ DxuiListView::Column { L"Address", 0 } });
            each->SetRows    ({ { DxuiListView::Cell { L"0300", false } } });
            each->Layout     (RECT { 0, 0, 200, 100 }, scaler);
        }

        code.SetTextViewSurface (true);
        code.Paint (codePainter, text, theme);
        list.Paint (listPainter, text, theme);

        Assert::IsNotNull (FindFill (codePainter, RecordedPaintKind::FillRect, 0xFFFFFFFFu), L"the code list's rows are the text view's");
        Assert::IsNotNull (FindFill (listPainter, RecordedPaintKind::FillRect, 0xFFF9F9F9u), L"any other list's are the content color");
    }
};
