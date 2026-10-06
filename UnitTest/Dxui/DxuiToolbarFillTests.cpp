#include "Pch.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  StretchyEntry
//
//  A custom entry with a length of its own that can shrink to a minimum, as
//  a row of pictures can.
//
////////////////////////////////////////////////////////////////////////////////

class StretchyEntry : public IDxuiToolbarCustomEntry
{
public:
    StretchyEntry (int widthPx, int minPx) : m_widthPx (widthPx), m_minPx (minPx) {}

    int              GetWidthPx    (bool, const DxuiDpiScaler &, IDxuiTextRenderer *) const override { return m_widthPx; }
    int              GetMinWidthPx (const DxuiDpiScaler &) const                             override { return m_minPx; }
    void             Layout        (const RECT & rc, bool, const DxuiDpiScaler &)           override { laidOut = rc; }
    void             Paint         (IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &, bool, bool, bool) override {}
    const wchar_t *  GetTooltipAt  (int, int, RECT &) const                                 override { return nullptr; }
    bool             OnClick       (int, int)                                               override { return false; }

    RECT  laidOut = {};

private:
    int  m_widthPx = 0;
    int  m_minPx   = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarFillTests
//
//  A toolbar whose one entry fills it, such as the history timeline: the
//  entry runs to the strip's far end, the toolbar runs the whole length of
//  whichever edge it docks to, and torn off it keeps its length, saved with
//  its place, in a window that resizes along that length only.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarFillTests)
{
public:

    static constexpr int   kEntryPx  = 300;
    static constexpr int   kMinPx    = 40;
    static constexpr int   kEntryId  = 1;
    static constexpr RECT  kArea     = { 0, 100, 1000, 800 };


    static void SetUpBar (DxuiToolbar & bar, StretchyEntry & custom, bool fill)
    {
        std::vector<DxuiToolbar::Entry>  entries (1);
        auto                             command = std::make_shared<DxuiCommand>();

        command->id    = kEntryId;
        command->label = L"History timeline";

        entries[0].command       = command;
        entries[0].custom        = &custom;
        entries[0].neverOverflow = true;
        entries[0].fill          = fill;

        bar.SetGrabHandle (true);
        bar.SetEntries    (std::move (entries));
    }


    static DxuiToolbarDock MakeDock (DxuiToolbarDock::Edge edge, int offsetDip)
    {
        DxuiToolbarDock  dock;

        dock.edge      = edge;
        dock.offsetDip = offsetDip;
        return dock;
    }


    TEST_METHOD (AFillingEntryRunsToTheFarEndOfTheStrip)
    {
        DxuiToolbar    bar;
        StretchyEntry  custom (kEntryPx, kMinPx);
        DxuiDpiScaler  scaler;
        int            barPad = 0;

        scaler.SetDpi (96);
        SetUpBar (bar, custom, true);

        barPad = bar.GetSpacingDp (DxuiToolbar::Spacing::BarPadX);

        bar.Layout (RECT { 0, 0, 2000, 82 }, scaler);
        Assert::AreEqual (2000 - barPad, (int) custom.laidOut.right, L"lying down, the entry reaches the strip's right end");

        bar.SetVertical (true);
        bar.Layout      (RECT { 0, 0, 82, 900 }, scaler);
        Assert::AreEqual (900 - barPad, (int) custom.laidOut.bottom, L"standing up, it reaches the strip's bottom");
    }


    TEST_METHOD (AnEntryThatDoesNotFillKeepsItsOwnLength)
    {
        DxuiToolbar    bar;
        StretchyEntry  custom (kEntryPx, kMinPx);
        DxuiDpiScaler  scaler;

        scaler.SetDpi (96);
        SetUpBar   (bar, custom, false);
        bar.Layout (RECT { 0, 0, 2000, 82 }, scaler);

        Assert::AreEqual (kEntryPx, (int) (custom.laidOut.right - custom.laidOut.left), L"no growth without fill");
    }


    TEST_METHOD (TheNaturalLengthOfAFillingToolbarIsTheEntryOwnLength)
    {
        DxuiToolbar    bar;
        StretchyEntry  custom (kEntryPx, kMinPx);
        DxuiDpiScaler  scaler;
        int            natural = 0;
        int            barPad  = 0;

        scaler.SetDpi (96);
        SetUpBar (bar, custom, true);

        barPad = bar.GetSpacingDp (DxuiToolbar::Spacing::BarPadX);

        bar.Layout (RECT { 0, 0, 2000, 82 }, scaler);
        natural = bar.GetNaturalLengthPx (scaler);

        Assert::AreEqual (DxuiToolbar::kGripDp + barPad * 2 + kEntryPx, natural, L"measured without the room it was last given");
    }


    TEST_METHOD (ADockedFillingToolbarRunsTheWholeLengthOfEveryEdge)
    {
        constexpr int  kMargin = DxuiToolbarHost::kMarginDp;

        for (DxuiToolbarDock::Edge edge : { DxuiToolbarDock::Edge::Top, DxuiToolbarDock::Edge::Bottom, DxuiToolbarDock::Edge::Left, DxuiToolbarDock::Edge::Right })
        {
            DxuiToolbar      bar;
            StretchyEntry    custom (kEntryPx, kMinPx);
            DxuiToolbarHost  host;
            DxuiDpiScaler    scaler;
            RECT             bounds   = {};
            bool             vertical = edge == DxuiToolbarDock::Edge::Left || edge == DxuiToolbarDock::Edge::Right;
            int              barPad   = 0;

            scaler.SetDpi (96);
            SetUpBar (bar, custom, true);

            barPad = bar.GetSpacingDp (DxuiToolbar::Spacing::BarPadX);

            host.Attach       (nullptr, &bar, nullptr, nullptr);
            host.SetFillsEdge (true);
            host.SetDock      (MakeDock (edge, 120));
            host.Layout       (kArea, kArea, scaler);

            bounds = bar.GetBounds();

            if (vertical)
            {
                Assert::AreEqual (kArea.top,    bounds.top,    L"a side bar starts at the area's top, whatever its offset");
                Assert::AreEqual (kArea.bottom, bounds.bottom, L"and runs to its bottom");
                Assert::AreEqual ((int) bounds.bottom - barPad, (int) custom.laidOut.bottom, L"its entry runs to the bar's far end");
            }
            else
            {
                Assert::AreEqual (kArea.left + kMargin,  bounds.left,  L"a top or bottom bar starts a margin in, whatever its offset");
                Assert::AreEqual (kArea.right - kMargin, bounds.right, L"and runs to a margin short of the far end");
                Assert::AreEqual ((int) bounds.right - barPad, (int) custom.laidOut.right, L"its entry runs to the bar's far end");
            }
        }
    }


    TEST_METHOD (ADockedFillingToolbarFollowsTheWindowSize)
    {
        DxuiToolbar      bar;
        StretchyEntry    custom (kEntryPx, kMinPx);
        DxuiToolbarHost  host;
        DxuiDpiScaler    scaler;

        scaler.SetDpi (96);
        SetUpBar (bar, custom, true);

        host.Attach       (nullptr, &bar, nullptr, nullptr);
        host.SetFillsEdge (true);
        host.SetDock      (MakeDock (DxuiToolbarDock::Edge::Bottom, 0));
        host.Layout       (kArea, kArea, scaler);
        host.Layout       (RECT { 0, 100, 1600, 800 }, kArea, scaler);

        Assert::AreEqual (1600L - DxuiToolbarHost::kMarginDp, bar.GetBounds().right, L"a wider window, a longer bar");
    }


    TEST_METHOD (AFloatingLengthIsSavedWithThePlace)
    {
        DxuiToolbarDock  dock;
        DxuiToolbarDock  read;

        dock.floating       = true;
        dock.floatPx        = POINT { 300, -40 };
        dock.floatLengthDip = 640;

        Assert::AreEqual (std::wstring (L"float 300 -40 length 640"), dock.ToText());

        read = DxuiToolbarDock::FromText (dock.ToText());
        Assert::IsTrue (read == dock, L"read back as written");

        dock.floatVertical = true;
        Assert::AreEqual (std::wstring (L"float 300 -40 vertical length 640"), dock.ToText());

        read = DxuiToolbarDock::FromText (dock.ToText());
        Assert::IsTrue   (read == dock, L"standing on end, read back as written");
        Assert::AreEqual (640, read.floatLengthDip);

        read = DxuiToolbarDock::FromText (L"float 300 -40 vertical");
        Assert::AreEqual (0,    read.floatLengthDip, L"a place saved without a length keeps the natural length");
        Assert::IsTrue   (read.floating && read.floatVertical);

        read = DxuiToolbarDock::FromText (L"float 300 -40 length x");
        Assert::IsFalse  (read.floating, L"a length that does not read gives the default place");
    }


    TEST_METHOD (ATornOffFillingToolbarKeepsItsLength)
    {
        DxuiToolbarDock  dock;

        Assert::AreEqual (656, DxuiToolbarHost::GetTearOffLengthDip (RECT { 8, 100, 992, 182 },  false, 144), L"984 pixels across at 150%");
        Assert::AreEqual (700, DxuiToolbarHost::GetTearOffLengthDip (RECT { 0, 100, 82, 800 },   true,  96),  L"standing up, its height");

        dock.floating       = true;
        dock.floatLengthDip = 656;

        Assert::AreEqual (984, DxuiToolbarHost::GetFloatLengthPx (dock, true,  144), L"the kept length, at the DPI");
        Assert::AreEqual (0,   DxuiToolbarHost::GetFloatLengthPx (dock, false, 144), L"a toolbar that does not fill takes its natural length");

        dock.floatLengthDip = 0;
        Assert::AreEqual (0,   DxuiToolbarHost::GetFloatLengthPx (dock, true,  144), L"and so does one with no length kept");
    }


    TEST_METHOD (AFloatingFillingToolbarResizesAtItsEndsOnly)
    {
        constexpr SIZE  kFlat    = { 600, 82 };
        constexpr SIZE  kUpright = { 82, 600 };
        constexpr int   kEnd     = 4;

        Assert::AreEqual ((LRESULT) HTLEFT,    DxuiToolbarWindow::ClassifyLengthResize (POINT { 1,   40 }, kFlat, false, kEnd), L"the left end");
        Assert::AreEqual ((LRESULT) HTRIGHT,   DxuiToolbarWindow::ClassifyLengthResize (POINT { 598, 40 }, kFlat, false, kEnd), L"the right end");
        Assert::AreEqual ((LRESULT) HTLEFT,    DxuiToolbarWindow::ClassifyLengthResize (POINT { 1,   1  }, kFlat, false, kEnd), L"a corner is its end, not a diagonal");
        Assert::AreEqual ((LRESULT) HTCLIENT,  DxuiToolbarWindow::ClassifyLengthResize (POINT { 300, 1  }, kFlat, false, kEnd), L"the top edge does not resize");
        Assert::AreEqual ((LRESULT) HTCLIENT,  DxuiToolbarWindow::ClassifyLengthResize (POINT { 300, 80 }, kFlat, false, kEnd), L"nor the bottom edge");
        Assert::AreEqual ((LRESULT) HTCLIENT,  DxuiToolbarWindow::ClassifyLengthResize (POINT { 300, 40 }, kFlat, false, kEnd), L"inside is the toolbar's");

        Assert::AreEqual ((LRESULT) HTTOP,     DxuiToolbarWindow::ClassifyLengthResize (POINT { 40, 1   }, kUpright, true, kEnd), L"standing up, the top end");
        Assert::AreEqual ((LRESULT) HTBOTTOM,  DxuiToolbarWindow::ClassifyLengthResize (POINT { 40, 598 }, kUpright, true, kEnd), L"the bottom end");
        Assert::AreEqual ((LRESULT) HTCLIENT,  DxuiToolbarWindow::ClassifyLengthResize (POINT { 1,  300 }, kUpright, true, kEnd), L"the left edge does not resize");
        Assert::AreEqual ((LRESULT) HTCLIENT,  DxuiToolbarWindow::ClassifyLengthResize (POINT { 80, 300 }, kUpright, true, kEnd), L"nor the right edge");

        Assert::AreEqual ((LRESULT) HTNOWHERE, DxuiToolbarWindow::ClassifyLengthResize (POINT { 700, 40 }, kFlat, false, kEnd), L"outside the window");
    }


    TEST_METHOD (TheEndsAreRoomyAndTheToolbarSitsBetweenThem)
    {
        constexpr RECT  kClient = { 0, 0, 600, 82 };
        constexpr RECT  kTall   = { 0, 0, 82, 600 };
        constexpr int   kEnd    = 10;

        RECT  flat    = DxuiToolbarWindow::GetToolbarRect (kClient, false, kEnd);
        RECT  upright = DxuiToolbarWindow::GetToolbarRect (kTall,   true,  kEnd);
        RECT  whole   = DxuiToolbarWindow::GetToolbarRect (kClient, false, 0);

        Assert::IsTrue (DxuiToolbarWindow::kResizeEndDp >= 8, L"an end is wide enough to find with a real mouse");

        Assert::AreEqual (10L,  flat.left,      L"lying down, the toolbar starts past the left end");
        Assert::AreEqual (590L, flat.right,     L"and stops short of the right end");
        Assert::AreEqual (0L,   flat.top,       L"and keeps the whole thickness");
        Assert::AreEqual (82L,  flat.bottom);

        Assert::AreEqual (10L,  upright.top,    L"standing up, it starts below the top end");
        Assert::AreEqual (590L, upright.bottom, L"and stops above the bottom end");
        Assert::AreEqual (0L,   upright.left);
        Assert::AreEqual (82L,  upright.right);

        Assert::IsTrue (whole.left == 0 && whole.right == 600, L"a window that does not resize gives the toolbar all of it");
    }


    TEST_METHOD (DraggingAnEndResizesTheWindowAlongItsLengthOnly)
    {
        constexpr RECT  kStart = { 100, 200, 700, 282 };
        constexpr RECT  kTall  = { 100, 200, 182, 800 };
        constexpr int   kMin   = 82;

        RECT  right  = DxuiToolbarWindow::GetResizedRect (kStart, HTRIGHT,  150,  kMin);
        RECT  left   = DxuiToolbarWindow::GetResizedRect (kStart, HTLEFT,   -50,  kMin);
        RECT  floor  = DxuiToolbarWindow::GetResizedRect (kStart, HTRIGHT,  -900, kMin);
        RECT  floorL = DxuiToolbarWindow::GetResizedRect (kStart, HTLEFT,   900,  kMin);
        RECT  bottom = DxuiToolbarWindow::GetResizedRect (kTall,  HTBOTTOM, 40,   kMin);
        RECT  top    = DxuiToolbarWindow::GetResizedRect (kTall,  HTTOP,    40,   kMin);

        Assert::IsTrue (right.left == 100 && right.right == 850 && right.top == 200 && right.bottom == 282, L"the right end follows the pointer");
        Assert::IsTrue (left.left == 50 && left.right == 700,                                                L"the left end does, and the right end stays");
        Assert::AreEqual (kMin, (int) (floor.right - floor.left),                                           L"never shorter than the floor");
        Assert::AreEqual (100L, floor.left);
        Assert::AreEqual (kMin, (int) (floorL.right - floorL.left),                                         L"from the left end too");
        Assert::AreEqual (700L, floorL.right,                                                               L"which keeps the right end put");
        Assert::IsTrue (bottom.top == 200 && bottom.bottom == 840 && bottom.left == 100 && bottom.right == 182, L"standing up, the bottom end");
        Assert::IsTrue (top.top == 240 && top.bottom == 800,                                                 L"and the top end");
    }


    TEST_METHOD (AnEndShowsTheSizingCursor)
    {
        Assert::IsTrue (DxuiToolbarWindow::GetResizeCursor (HTLEFT)   == IDC_SIZEWE, L"left end");
        Assert::IsTrue (DxuiToolbarWindow::GetResizeCursor (HTRIGHT)  == IDC_SIZEWE, L"right end");
        Assert::IsTrue (DxuiToolbarWindow::GetResizeCursor (HTTOP)    == IDC_SIZENS, L"top end");
        Assert::IsTrue (DxuiToolbarWindow::GetResizeCursor (HTBOTTOM) == IDC_SIZENS, L"bottom end");
        Assert::IsTrue (DxuiToolbarWindow::GetResizeCursor (HTCLIENT) == nullptr,    L"the toolbar has its own");
    }
};
