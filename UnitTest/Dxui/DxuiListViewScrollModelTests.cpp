#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewScrollModelTests
//
//  A list whose vertical bar is drawn from a host's scroll model, as a
//  disassembly's is from the address space. The bar is visible over rows
//  that fit, the thumb is placed by the model's position, and clicks on the
//  arrows and track and drags of the thumb call the host with scroll codes
//  instead of moving the rows.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewScrollModelTests)
{
public:

    struct Report
    {
        int  code = -1;
        int  pos  = -1;
    };



    static SCROLLINFO MakeModel (int pos)
    {
        SCROLLINFO  model = {};

        model.cbSize = sizeof (model);
        model.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
        model.nMin   = 0;
        model.nMax   = 0x10000;
        model.nPage  = 0x40;
        model.nPos   = pos;

        return model;
    }



    static void ConfigureList (DxuiListView & list, std::vector<Report> & reports, int pos)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<DxuiListView::Column>             cols;
        std::vector<std::vector<DxuiListView::Cell>>  data;
        int                                           i = 0;

        scaler.SetDpi (96);

        cols.push_back (DxuiListView::Column { L"Instruction", 200 });

        for (i = 0; i < 5; i++)
        {
            data.push_back ({ DxuiListView::Cell { std::format (L"row{}", i), false } });
        }

        list.SetColumns         (std::move (cols));
        list.SetShowHeader      (false);
        list.SetRows            (std::move (data));
        list.SetVertScrollModel (MakeModel (pos), [&reports] (int code, int at) { reports.push_back (Report { code, at }); });
        list.Layout             (RECT { 0, 0, 400, 300 }, scaler);
    }



    static void Press (DxuiListView & list, int x, int y)
    {
        DxuiMouseEvent  ev;

        ev.kind        = DxuiMouseEventKind::Down;
        ev.button      = DxuiMouseButton::Left;
        ev.positionDip = POINT { x, y };
        list.OnMouse (ev);

        ev.kind = DxuiMouseEventKind::Up;
        list.OnMouse (ev);
    }



    TEST_METHOD (TheBarIsShownForRowsThatFit)
    {
        DxuiListView         list;
        std::vector<Report>  reports;

        ConfigureList (list, reports, 0x6000);

        Assert::IsTrue (list.IsScrollbarVisible(), L"The model's range is wider than its page");
    }



    TEST_METHOD (TheThumbFollowsTheModelsPosition)
    {
        DxuiListView         low;
        DxuiListView         high;
        std::vector<Report>  reports;

        ConfigureList (low,  reports, 0x2000);
        ConfigureList (high, reports, 0xC000);

        Assert::IsTrue (low.GetScrollbarGeometry().thumbTop < high.GetScrollbarGeometry().thumbTop);
    }



    TEST_METHOD (AnArrowReportsALineAndLeavesTheRows)
    {
        DxuiListView                        list;
        std::vector<Report>                 reports;
        DxuiListView::ScrollbarMetrics      bar;

        ConfigureList (list, reports, 0x6000);
        bar = list.GetScrollbarGeometry();

        Press (list, bar.barX + bar.barW / 2, bar.downArrowTop + bar.arrowH / 2);
        Press (list, bar.barX + bar.barW / 2, bar.upArrowTop + bar.arrowH / 2);

        Assert::AreEqual ((size_t) 2, reports.size());
        Assert::AreEqual ((int) SB_LINEDOWN, reports[0].code);
        Assert::AreEqual ((int) SB_LINEUP,   reports[1].code);
        Assert::AreEqual (0, list.GetTopRow(), L"The rows are the host's to move");
    }



    TEST_METHOD (ATrackClickReportsAPage)
    {
        DxuiListView                        list;
        std::vector<Report>                 reports;
        DxuiListView::ScrollbarMetrics      bar;

        ConfigureList (list, reports, 0x6000);
        bar = list.GetScrollbarGeometry();

        list.GetPageFromTrackClick (bar.trackTop + 1);
        list.GetPageFromTrackClick (bar.trackTop + bar.trackH - 1);

        Assert::AreEqual ((size_t) 2, reports.size());
        Assert::AreEqual ((int) SB_PAGEUP,   reports[0].code);
        Assert::AreEqual ((int) SB_PAGEDOWN, reports[1].code);
    }



    TEST_METHOD (AThumbDragReportsAPositionInTheModel)
    {
        DxuiListView                        list;
        std::vector<Report>                 reports;
        DxuiListView::ScrollbarMetrics      bar;
        int                                 grab = 0;

        ConfigureList (list, reports, 0x6000);
        bar  = list.GetScrollbarGeometry();
        grab = (int) (bar.thumbTop + bar.thumbH / 2.0f);

        list.BeginThumbDrag  (grab);
        list.UpdateThumbDrag (bar.trackTop + bar.trackH * 4);
        list.EndThumbDrag();

        Assert::IsFalse  (reports.empty());
        Assert::AreEqual ((int) SB_THUMBTRACK, reports.back().code);
        Assert::AreEqual (0x10000 - 0x40, reports.back().pos, L"A drag past the end returns the model's last page");
    }



    TEST_METHOD (ClearingTheModelReturnsTheBarToTheRows)
    {
        DxuiListView         list;
        std::vector<Report>  reports;

        ConfigureList (list, reports, 0x6000);
        list.SetVertScrollModel (std::nullopt, nullptr);

        Assert::IsFalse (list.IsScrollbarVisible(), L"Five rows fit without a bar");
    }
};
