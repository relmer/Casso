#include "Pch.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListViewColumnFitTests
//
//  Column widths belong to the view rather than to what it is showing: a
//  refill leaves them where they are, and a divider double-click fits one to
//  its content on demand.
//
//  Fitting needs the text renderer, which only the paint pass holds, so a fit
//  is asked for by the click and taken on the next paint.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiListViewColumnFitTests)
{
public:

    static constexpr int  kDividerX = 200;   // the Name column's right edge



    struct Fixture
    {
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        DxuiListView          list;

        void  Build (const wchar_t * name)
        {
            DxuiDpiScaler                                 scaler;
            std::vector<DxuiListView::Column>             cols;
            std::vector<std::vector<DxuiListView::Cell>>  data;
            int                                           i = 0;

            scaler.SetDpi (96);

            cols.push_back (DxuiListView::Column { L"Name", 200 });
            cols.push_back (DxuiListView::Column { L"Size", 100 });

            for (i = 0; i < 40; i++)
            {
                data.push_back ({ DxuiListView::Cell { name, false },
                                  DxuiListView::Cell { L"1 KB", false } });
            }

            list.SetColumns        (std::move (cols));
            list.SetShowHeader     (true);
            list.SetPreciseAutoFit (true);   // as the file browser does
            list.SetRows           (std::move (data));
            list.Layout            (RECT { 0, 0, 600, 400 }, scaler);
        }

        void  Paint()
        {
            painter.Reset();
            list.Paint (painter, text, theme);
        }

        void  PressDivider()
        {
            DxuiMouseEvent  ev;

            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { kDividerX, 10 };   // inside the header strip

            ev.kind = DxuiMouseEventKind::Down;
            list.OnMouse (ev);

            ev.kind = DxuiMouseEventKind::Up;
            list.OnMouse (ev);
        }
    };



    TEST_METHOD (ARefillLeavesTheColumnsWhereTheyAre)
    {
        Fixture                                       f;
        std::vector<std::vector<DxuiListView::Cell>>  wider;
        int                                           before = 0;
        int                                           i      = 0;

        f.Build (L"short.txt");
        f.Paint();

        //  The columns' own total, which grows if a refill re-measures them.
        before = f.list.GetContentWidthPx();

        Assert::IsTrue (before > 0, L"The columns have been measured once");

        //  A folder whose names are far longer arrives.
        for (i = 0; i < 40; i++)
        {
            wider.push_back ({ DxuiListView::Cell { L"a-very-much-longer-file-name-indeed.txt", false },
                               DxuiListView::Cell { L"1 KB", false } });
        }

        f.list.SetRows (std::move (wider));
        f.Paint();

        Assert::AreEqual (before, f.list.GetContentWidthPx(),
                          L"New rows do not re-size the columns");
    }



    TEST_METHOD (ADividerDoubleClickFitsThatColumn)
    {
        Fixture  f;

        f.Build (L"a-name-wider-than-the-column-was-given.txt");
        f.Paint();

        Assert::AreEqual (-1, f.list.GetColumnOverrideWidthPx (0), L"No width of its own yet");

        //  Two presses on the divider inside the double-click time.
        f.PressDivider();
        f.PressDivider();

        //  The fit is taken on the next paint, once the content is measured.
        f.Paint();

        Assert::IsTrue (f.list.GetColumnOverrideWidthPx (0) > 0,
                        L"The column now has a width of its own, fitted to its content");
    }



    //  The file browser's columns: a stretch Name column first, then others.
    static void  BuildWithStretchName (DxuiListView & list)
    {
        DxuiDpiScaler                                 scaler;
        std::vector<std::vector<DxuiListView::Cell>>  rows;
        int                                           i = 0;

        scaler.SetDpi (96);

        for (i = 0; i < 20; i++)
        {
            rows.push_back ({ DxuiListView::Cell { L"file.txt", false }, DxuiListView::Cell { L"TXT", false } });
        }

        list.SetColumns    ({ DxuiListView::Column { L"Name", 200, true }, DxuiListView::Column { L"Type", 100 } });
        list.SetShowHeader (true);
        list.SetRows       (std::move (rows));
        list.Layout        (RECT { 0, 0, 600, 400 }, scaler);
    }



    static int  FindDividerX (const DxuiListView & list, int column)
    {
        int  x = 0;

        for (x = 0; x < 600; x++)
        {
            if (list.HitTestColumnResize (x, 10, 4) == column)
            {
                return x;
            }
        }

        return -1;
    }



    TEST_METHOD (TheStretchColumnsDividerCanBeGrabbed)
    {
        DxuiListView  list;
        int           x = 0;

        BuildWithStretchName (list);

        x = FindDividerX (list, 0);

        //  Name absorbs the spare width by default, which is no reason it
        //  cannot be given a width of its own.
        Assert::IsTrue (x >= 0, L"The Name column's divider can be grabbed");
        Assert::IsTrue (list.GetCursorForPoint (POINT { x, 10 }) == IDC_SIZEWE, L"and shows the resize cursor");
    }



    TEST_METHOD (DraggingTheStretchColumnsDivider_ChangesItsWidth)
    {
        DxuiListView    list;
        DxuiMouseEvent  ev;
        int             x = 0;

        BuildWithStretchName (list);

        x = FindDividerX (list, 0);
        Assert::IsTrue (x >= 0);

        ev.button = DxuiMouseButton::Left;

        ev.kind        = DxuiMouseEventKind::Down;
        ev.positionDip = POINT { x, 10 };
        list.OnMouse (ev);

        ev.kind        = DxuiMouseEventKind::Move;
        ev.positionDip = POINT { x - 60, 10 };
        list.OnMouse (ev);

        ev.kind = DxuiMouseEventKind::Up;
        list.OnMouse (ev);

        Assert::IsTrue (list.GetColumnOverrideWidthPx (0) > 0, L"The drag gave Name a width of its own");
    }



    TEST_METHOD (ASinglePressOnADividerFitsNothing)
    {
        Fixture  f;

        f.Build (L"a-name-wider-than-the-column-was-given.txt");
        f.Paint();

        f.PressDivider();
        f.Paint();

        Assert::AreEqual (-1, f.list.GetColumnOverrideWidthPx (0),
                          L"One press starts a drag rather than fitting");
    }
};
