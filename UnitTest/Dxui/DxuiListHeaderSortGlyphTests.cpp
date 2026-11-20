#include "Pch.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListHeaderSortGlyphTests
//
//  The details header's sort chevron, as File Explorer draws it: centered
//  over the sorted column, with its top half a DIP below the header's top
//  edge, well clear of the title under it.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListHeaderSortGlyphTests)
{
public:

    static constexpr int  kNameWidthDip = 200;
    static constexpr int  kSizeWidthDip = 100;



    //  The lines painted over the Size column's middle, the chevron's two
    //  strokes, as one bounding box: top and bottom in pixels.
    static void  PaintSortedBySize (UINT dpi, float & outTop, float & outBottom, int & outLines)
    {
        MockDxuiPainter                               painter;
        MockDxuiTextRenderer                          text;
        MockDxuiTheme                                 theme;
        DxuiListView                                  list;
        DxuiDpiScaler                                 scaler;
        std::vector<DxuiListView::Column>             cols;
        std::vector<std::vector<DxuiListView::Cell>>  data;
        float                                         middle = 0.0f;
        float                                         span   = 0.0f;



        scaler.SetDpi (dpi);

        cols.push_back (DxuiListView::Column { L"Name", kNameWidthDip });
        cols.push_back (DxuiListView::Column { L"Size", kSizeWidthDip });
        data.push_back ({ DxuiListView::Cell { L"A", false }, DxuiListView::Cell { L"1 KB", false } });

        list.SetColumns       (std::move (cols));
        list.SetShowHeader    (true);
        list.SetRows          (std::move (data));
        list.SetSortIndicator (1, false);
        list.Layout           (RECT { 0, 0, scaler.ToPx (600), scaler.ToPx (400) }, scaler);
        list.Paint            (painter, text, theme);

        middle    = scaler.ToPxf ((float) kNameWidthDip + (float) kSizeWidthDip / 2.0f);
        span      = scaler.ToPxf (5.0f);
        outTop    = 1e9f;
        outBottom = -1e9f;
        outLines  = 0;

        for (const RecordedPaintCall & call : painter.Calls())
        {
            bool  isOverMiddle = call.x >= middle - span && call.x + call.width <= middle + span;

            if (call.kind == RecordedPaintKind::DrawLine && isOverMiddle && call.width > 0.0f)
            {
                outTop    = (std::min) (outTop, call.y);
                outBottom = (std::max) (outBottom, call.y + call.height);
                outLines++;
            }
        }
    }




    TEST_METHOD (TheSortChevron_IsAtTheHeadersTopEdge_At100And150Percent)
    {
        for (UINT dpi : { 96u, 144u })
        {
            DxuiDpiScaler  scaler;
            float          top    = 0.0f;
            float          bottom = 0.0f;
            int            lines  = 0;



            scaler.SetDpi (dpi);
            PaintSortedBySize (dpi, top, bottom, lines);

            Assert::AreEqual (2, lines, std::format (L"the chevron's two strokes at {} dpi", dpi).c_str());
            Assert::IsTrue   (top >= 0.0f && top <= scaler.ToPxf (1.0f), std::format (L"its top within a DIP of the header's top edge at {} dpi: {}", dpi, top).c_str());
            Assert::IsTrue   (bottom <= scaler.ToPxf (4.0f), std::format (L"its bottom within 4 DIP of the edge at {} dpi: {}", dpi, bottom).c_str());
        }
    }
};
