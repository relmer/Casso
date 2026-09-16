#include "Pch.h"

#include "Widgets/DxuiHexView.h"
#include "Widgets/DxuiListView.h"
#include "Widgets/DxuiTextView.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbarEdgeGrabTests
//
//  A scrollbar against the window's right edge has to claim the pointer over
//  the window's resize band, and the window asks each control in the window's
//  own client coordinates. A control laid out at the client origin cannot tell
//  those from its own, so every view here is laid out where a preview pane
//  sits: away from the origin, flush with the right edge.
//
//  EACH VIEW IS PAINTED FIRST, as it always has been by the time a pointer
//  reaches it. Some views place their bar's track while painting, and asking
//  an unpainted one only tests a state the window never shows.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiScrollbarEdgeGrabTests)
{
public:

    //  Where a preview pane sits in a 1900-wide window.
    static constexpr LONG  kLeft   = 1300;
    static constexpr LONG  kTop    = 180;
    static constexpr LONG  kRight  = 1900;
    static constexpr LONG  kBottom = 850;

    //  Inside a 10-DIP bar at the right edge, halfway down.
    static constexpr int   kBarX   = kRight - 4;
    static constexpr int   kBarY   = (kTop + kBottom) / 2;



    class ByteRun : public IDxuiHexSource
    {
    public:
        uint64_t  GetByteCount() const override { return 64 * 1024; }

        void  ReadBytes (uint64_t offset, std::span<uint8_t> out) const override
        {
            for (size_t i = 0; i < out.size(); i++)
            {
                out[i] = (uint8_t) ((offset + i) & 0xFF);
            }
        }
    };



    static DxuiDpiScaler  Scaler96()
    {
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);

        return scaler;
    }



    static void  PaintOnce (IDxuiControl & control)
    {
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;

        control.Paint (painter, text, theme);
    }



    TEST_METHOD (ListView_ClaimsItsBarInClientCoordinates)
    {
        DxuiListView                                  list;
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        int                                           i = 0;

        for (i = 0; i < 300; i++)
        {
            rows.push_back ({ DxuiListView::Cell { L"row", false } });
        }

        list.SetColumns ({ DxuiListView::Column { L"Name", 200 } });
        list.SetRows    (std::move (rows));
        list.Layout     (RECT { kLeft, kTop, kRight, kBottom }, Scaler96());
        PaintOnce (list);

        //  The control this file compares the others against.
        Assert::IsTrue (list.IsOverScrollbar (POINT { kBarX, kBarY }));
    }



    TEST_METHOD (TextView_ClaimsItsBarInClientCoordinates)
    {
        DxuiTextView                    view;
        std::vector<DxuiTextView::Row>  rows;
        int                             i = 0;

        for (i = 0; i < 400; i++)
        {
            DxuiTextView::Row  row;

            row.cells = { std::format (L"line {}", i) };
            rows.push_back (row);
        }

        view.SetCellSize (8, 16);
        view.Layout      (RECT { kLeft, kTop, kRight, kBottom }, Scaler96());
        view.SetRows     (std::move (rows));
        PaintOnce (view);

        Assert::IsTrue (view.IsScrollbarVisible(), L"Four hundred lines need the bar");
        Assert::IsTrue (view.IsOverScrollbar (POINT { kBarX, kBarY }),
                        L"A point on the bar, in the window's coordinates, is on the bar");
    }



    TEST_METHOD (HexView_ClaimsItsBarInClientCoordinates)
    {
        DxuiHexView  view;
        ByteRun      bytes;

        view.SetSource (&bytes);
        view.Layout    (RECT { kLeft, kTop, kRight, kBottom }, Scaler96());
        PaintOnce (view);

        Assert::IsTrue (view.IsOverScrollbar (POINT { kBarX, kBarY }),
                        L"A point on the bar, in the window's coordinates, is on the bar");
    }
};
