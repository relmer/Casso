#include "Pch.h"

#include "MockDxuiTextRenderer.h"



using namespace Microsoft::VisualStudio::CppUnitTestFramework;



// Shared by both TEST_CLASSes below, so these live at file scope rather than
// on either one. `static` supplies the internal linkage the anonymous
// namespace was there for.
static RECT  MakeRect (int l, int t, int r, int b) { return RECT{ l, t, r, b }; }





////////////////////////////////////////////////////////////////////////////////
//
//  AssertRect
//
////////////////////////////////////////////////////////////////////////////////

static void  AssertRect (const RECT & expected, const RECT & actual, const wchar_t * msg)
{
    Assert::AreEqual (expected.left,   actual.left,   msg);
    Assert::AreEqual (expected.top,    actual.top,    msg);
    Assert::AreEqual (expected.right,  actual.right,  msg);
    Assert::AreEqual (expected.bottom, actual.bottom, msg);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TEST_CLASS
//
//
//   Covers the button-row customization added for the SettingsPanel ->
//   DxuiPropertySheet migration (T162): the hideable Apply button + custom
//   OK label / width that let the sheet express Casso's no-Apply /
//   "OK (reboot)" model. The reflow math is exercised through the pure
//   LayoutButtonRow helper so no window is required.
//
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPropertySheetTests)
{
public:
    TEST_METHOD (Defaults_ApplyVisibleOkTextOkWidth)
    {
        DxuiPropertySheet  sheet;

        Assert::IsTrue   (sheet.ApplyVisible());
        Assert::AreEqual (L"OK", sheet.GetOkText().c_str());
        Assert::AreEqual (0, sheet.GetOkWidthDip());
    }


    TEST_METHOD (Setters_UpdateState)
    {
        DxuiPropertySheet  sheet;

        sheet.SetApplyVisible (false);
        sheet.SetOkText       (L"OK (reboot)");
        sheet.SetOkWidthDip   (140);

        Assert::IsFalse  (sheet.ApplyVisible());
        Assert::AreEqual (L"OK (reboot)", sheet.GetOkText().c_str());
        Assert::AreEqual (140, sheet.GetOkWidthDip());
    }


    TEST_METHOD (LayoutButtonRow_ThreeEqual_RightAligned96Dpi)
    {
        DxuiDpiScaler        scaler;
        RECT                 bounds    = MakeRect (0, 0, 400, 300);
        int                  widths[3] = { 96, 96, 96 };
        std::array<RECT, 3>  rects     = {};

        scaler.SetDpi (96);
        DxuiPropertySheet::LayoutButtonRow (bounds, scaler, widths, rects);

        AssertRect (MakeRect (80,  256, 176, 284), rects[0], L"ok");
        AssertRect (MakeRect (184, 256, 280, 284), rects[1], L"cancel");
        AssertRect (MakeRect (288, 256, 384, 284), rects[2], L"apply");

        // Rightmost button hugs the edge pad (16 DIP @ 96 dpi).
        Assert::AreEqual (bounds.right - 16, rects[2].right);
    }


    TEST_METHOD (LayoutButtonRow_ApplyHidden_TwoButtonsReflowRight)
    {
        DxuiDpiScaler        scaler;
        RECT                 bounds    = MakeRect (0, 0, 400, 300);
        int                  widths[2] = { 96, 96 };
        std::array<RECT, 2>  rects     = {};

        scaler.SetDpi (96);
        DxuiPropertySheet::LayoutButtonRow (bounds, scaler, widths, rects);

        AssertRect (MakeRect (184, 256, 280, 284), rects[0], L"ok");
        AssertRect (MakeRect (288, 256, 384, 284), rects[1], L"cancel");

        // Cancel is now the rightmost button and still hugs the edge pad.
        Assert::AreEqual (bounds.right - 16, rects[1].right);
    }


    TEST_METHOD (LayoutButtonRow_CustomOkWidth_WidensOkShiftsRow)
    {
        DxuiDpiScaler        scaler;
        RECT                 bounds    = MakeRect (0, 0, 400, 300);
        int                  widths[3] = { 140, 96, 96 };
        std::array<RECT, 3>  rects     = {};

        scaler.SetDpi (96);
        DxuiPropertySheet::LayoutButtonRow (bounds, scaler, widths, rects);

        AssertRect (MakeRect (36,  256, 176, 284), rects[0], L"ok");
        AssertRect (MakeRect (184, 256, 280, 284), rects[1], L"cancel");
        AssertRect (MakeRect (288, 256, 384, 284), rects[2], L"apply");

        Assert::AreEqual (140, (int) (rects[0].right - rects[0].left));
        Assert::AreEqual ((int) (bounds.right - 16), (int) rects[2].right);
    }


    TEST_METHOD (LayoutButtonRow_192Dpi_DoublesMetrics)
    {
        DxuiDpiScaler        scaler;
        RECT                 bounds    = MakeRect (0, 0, 800, 600);
        int                  widths[3] = { 96, 96, 96 };
        std::array<RECT, 3>  rects     = {};

        scaler.SetDpi (192);
        DxuiPropertySheet::LayoutButtonRow (bounds, scaler, widths, rects);

        AssertRect (MakeRect (160, 512, 352, 568), rects[0], L"ok");
        AssertRect (MakeRect (368, 512, 560, 568), rects[1], L"cancel");
        AssertRect (MakeRect (576, 512, 768, 568), rects[2], L"apply");

        Assert::AreEqual (bounds.right - 32, rects[2].right);
    }


    TEST_METHOD (PageScroll_OffByDefault)
    {
        DxuiPropertySheet  sheet;

        Assert::IsFalse  (sheet.IsPageScrollable());
        Assert::AreEqual (0, sheet.GetPageScrollPos());
    }


    TEST_METHOD (ClampScrollPos_KeepsWithinContent)
    {
        Assert::AreEqual (0,   DxuiPropertySheet::ClampScrollPos (-40, 1000, 800), L"never above the top");
        Assert::AreEqual (120, DxuiPropertySheet::ClampScrollPos (120, 1000, 800), L"in range is unchanged");
        Assert::AreEqual (200, DxuiPropertySheet::ClampScrollPos (900, 1000, 800), L"bottom stops at content - viewport");
        Assert::AreEqual (0,   DxuiPropertySheet::ClampScrollPos (50,  600,  800), L"content that fits never scrolls");
    }


    TEST_METHOD (ScrollPosToReveal_VisibleTargetLeavesPosition)
    {
        RECT  viewport = MakeRect (0, 100, 700, 500);

        Assert::AreEqual (60, DxuiPropertySheet::GetScrollPosToReveal (60, MakeRect (20, 200, 200, 230), viewport));
    }


    TEST_METHOD (ScrollPosToReveal_BelowScrollsDownByOvershoot)
    {
        RECT  viewport = MakeRect (0, 100, 700, 500);

        // Bottom at 540 is 40 past the viewport, so scroll 40 further.
        Assert::AreEqual (100, DxuiPropertySheet::GetScrollPosToReveal (60, MakeRect (20, 510, 200, 540), viewport));
    }


    TEST_METHOD (ScrollPosToReveal_AboveScrollsUpByUndershoot)
    {
        RECT  viewport = MakeRect (0, 100, 700, 500);

        // Top at 70 is 30 above the viewport, so scroll 30 back.
        Assert::AreEqual (30, DxuiPropertySheet::GetScrollPosToReveal (60, MakeRect (20, 70, 200, 100), viewport));
    }


    TEST_METHOD (ScrollPosToReveal_TallTargetShowsItsTop)
    {
        RECT  viewport = MakeRect (0, 100, 700, 500);

        // 600 px tall in a 400 px viewport: aligning the bottom would push the
        // top out, so the top edge is aligned instead.
        Assert::AreEqual (80, DxuiPropertySheet::GetScrollPosToReveal (60, MakeRect (20, 120, 200, 720), viewport));
    }


    TEST_METHOD (PropertyPage_ClipsOnlyOutsideItsViewport)
    {
        DxuiPropertyPage  page (L"Page");
        RECT              viewport = MakeRect (0, 100, 700, 500);

        Assert::IsFalse (page.IsPointClipped (POINT { 10, 10 }), L"no viewport, nothing clipped");

        page.SetViewport (&viewport);
        Assert::IsTrue  (page.IsPointClipped (POINT { 10, 50 }),  L"above the viewport");
        Assert::IsFalse (page.IsPointClipped (POINT { 10, 300 }), L"inside the viewport");
        Assert::IsTrue  (page.IsPointClipped (POINT { 10, 500 }), L"bottom edge is exclusive");

        page.SetViewport (nullptr);
        Assert::IsFalse (page.IsPointClipped (POINT { 10, 50 }), L"cleared viewport clips nothing");
    }


    //
    //  A page that reports a fixed content height, and a sheet laid out with
    //  no window, so the scroll range is driven only by what the page
    //  reports. With no window there is no design-height deficit.
    //
    class TallPage : public DxuiPropertyPage
    {
    public:
        TallPage (std::wstring title, int heightPx) : DxuiPropertyPage (std::move (title)), m_heightPx (heightPx) {}

        void  Layout (const RECT & rect, const DxuiDpiScaler & scaler) override
        {
            UNREFERENCED_PARAMETER (scaler);

            DxuiPanel::SetBounds (rect);
            SetContentHeightPx   (m_heightPx);
        }

        // The page's rows came or went after layout.
        void  Resize (int heightPx)
        {
            m_heightPx = heightPx;
            SetContentHeightPx (heightPx);
        }

    private:
        int  m_heightPx = 0;
    };


    class ScrollSheet : public DxuiPropertySheet
    {
    public:
        static constexpr int  kWidthPx  = 600;
        static constexpr int  kHeightPx = 400;

        TallPage *  AddPage (int heightPx) { return CreatePage<TallPage> (L"Page", heightPx); }

        void  LayOut()
        {
            DxuiDpiScaler  scaler;

            scaler.SetDpi (kScrollTestDpi);
            Layout (MakeRect (0, 0, kWidthPx, kHeightPx), scaler);
        }

        // The page area inside the viewport's padding.
        int  GetAreaTop() const    { return GetPageViewportPx().top    + DxuiButtonRow::kEdgePadDip; }
        int  GetAreaBottom() const { return GetPageViewportPx().bottom - DxuiButtonRow::kEdgePadDip; }
    };

    static constexpr UINT  kScrollTestDpi = 96;
    static constexpr int   kShortPagePx   = 100;
    static constexpr int   kTallPagePx    = 1000;
    static constexpr int   kFarDownPx     = 100000;
    static constexpr int   kOverhangPx    = 50;


    TEST_METHOD (PageScroll_ContentThatFitsDoesNotScroll)
    {
        ScrollSheet  sheet;



        sheet.AddPage (kShortPagePx);
        sheet.LayOut();

        Assert::IsFalse  (sheet.IsPageScrollable());
        Assert::AreEqual (0, sheet.GetPageScrollPos());
    }


    TEST_METHOD (PageScroll_TallContentScrollsToRevealItsLastRow)
    {
        ScrollSheet  sheet;
        TallPage   * page  = sheet.AddPage (kTallPagePx);
        int          viewH = 0;



        sheet.LayOut();
        viewH = sheet.GetAreaBottom() - sheet.GetAreaTop();

        Assert::IsTrue (sheet.IsPageScrollable(), L"a page taller than the viewport scrolls with no window height deficit");
        Assert::IsTrue (page->HasViewport(),      L"and is clipped to the viewport from its first layout");

        sheet.SetPageScrollPos (kFarDownPx);

        Assert::AreEqual (kTallPagePx - viewH, sheet.GetPageScrollPos(), L"the bottom stops at the page's content height");
        Assert::AreEqual (sheet.GetAreaBottom(), (int) page->GetBounds().top + kTallPagePx, L"the last row sits at the bottom of the viewport");
    }


    TEST_METHOD (PageScroll_FollowsContentHeightChangedAfterLayout)
    {
        ScrollSheet  sheet;
        TallPage   * page  = sheet.AddPage (kShortPagePx);
        int          viewH = 0;



        sheet.LayOut();
        viewH = sheet.GetAreaBottom() - sheet.GetAreaTop();
        Assert::IsFalse (sheet.IsPageScrollable());

        page->Resize (kTallPagePx);
        Assert::IsTrue (sheet.IsPageScrollable(), L"rows added after layout extend the range");

        sheet.SetPageScrollPos (kFarDownPx);
        page->Resize (viewH + kOverhangPx);

        Assert::AreEqual (kOverhangPx, sheet.GetPageScrollPos(), L"rows removed pull the position back into range");
        Assert::AreEqual (sheet.GetAreaTop() - kOverhangPx, (int) page->GetBounds().top, L"and the page is placed at the new position");

        page->Resize (kShortPagePx);
        Assert::IsFalse  (sheet.IsPageScrollable(), L"content that fits again stops scrolling");
        Assert::AreEqual (sheet.GetAreaTop(), (int) page->GetBounds().top);
    }


    TEST_METHOD (PageScroll_RangeIsTheActivePages)
    {
        ScrollSheet  sheet;



        sheet.AddPage (kShortPagePx);
        sheet.AddPage (kTallPagePx);
        sheet.LayOut();

        Assert::IsFalse (sheet.IsPageScrollable(), L"the short page is showing");

        sheet.SetActivePage (1);
        Assert::IsTrue (sheet.IsPageScrollable(), L"the tall page scrolls");

        sheet.SetActivePage (0);
        Assert::IsFalse (sheet.IsPageScrollable(), L"back on the short page");
    }

    //
    //  A page that reports a fixed content width and height.
    //
    class ExtentPage : public DxuiPropertyPage
    {
    public:
        ExtentPage (std::wstring title, int widthPx, int heightPx) : DxuiPropertyPage (std::move (title)), m_widthPx (widthPx), m_heightPx (heightPx) {}

        void  Layout (const RECT & rect, const DxuiDpiScaler & scaler) override
        {
            UNREFERENCED_PARAMETER (scaler);

            DxuiPanel::SetBounds (rect);
            SetContentWidthPx    (m_widthPx);
            SetContentHeightPx   (m_heightPx);
        }

    private:
        int  m_widthPx  = 0;
        int  m_heightPx = 0;
    };


    class ExtentSheet : public ScrollSheet
    {
    public:
        ExtentPage *  AddExtentPage (int widthPx, int heightPx) { return CreatePage<ExtentPage> (L"Page", widthPx, heightPx); }
    };


    static constexpr int  kNarrowPx  = 300;
    static constexpr int  kWidePx    = 650;
    static constexpr int  kCaptionPx = 32;
    static constexpr int  kGrowthPx  = 70;


    //  Content larger by some amount makes the maximum larger by exactly that
    //  much, and a caption adds its own height.
    TEST_METHOD (MaxClientSize_GrowsWithTheContentAndTheCaption)
    {
        DxuiDpiScaler  scaler;
        SIZE           base;
        SIZE           grown;
        SIZE           captioned;



        scaler.SetDpi (kScrollTestDpi);

        base      = DxuiPropertySheet::ComputeMaxClientSizePx (SIZE {}, SIZE { kNarrowPx, kShortPagePx }, 0, scaler);
        grown     = DxuiPropertySheet::ComputeMaxClientSizePx (SIZE {}, SIZE { kNarrowPx + kGrowthPx, kShortPagePx + kGrowthPx }, 0, scaler);
        captioned = DxuiPropertySheet::ComputeMaxClientSizePx (SIZE {}, SIZE { kNarrowPx, kShortPagePx }, kCaptionPx, scaler);

        Assert::AreEqual (kNarrowPx + DxuiButtonRow::kEdgePadDip * 2, (int) base.cx, L"the inset on either side of the content");
        Assert::AreEqual (base.cx + kGrowthPx, grown.cx);
        Assert::AreEqual (base.cy + kGrowthPx, grown.cy);
        Assert::AreEqual (base.cx,              captioned.cx);
        Assert::AreEqual (base.cy + kCaptionPx, captioned.cy);
    }


    //  Content smaller than the design never brings the maximum below it.
    TEST_METHOD (MaxClientSize_IsNeverBelowTheDesign)
    {
        DxuiDpiScaler  scaler;
        SIZE           maxPx;
        SIZE           designPx = { ScrollSheet::kWidthPx, kTallPagePx };



        scaler.SetDpi (kScrollTestDpi);
        maxPx = DxuiPropertySheet::ComputeMaxClientSizePx (designPx, SIZE { kNarrowPx, kShortPagePx }, 0, scaler);

        Assert::AreEqual (designPx.cx, maxPx.cx);
        Assert::AreEqual (designPx.cy, maxPx.cy);
    }


    //  The widest page sets the width and the tallest the height, even when
    //  they are different pages and neither is active.
    TEST_METHOD (MaxClientSize_TakesTheWidestAndTallestPages)
    {
        ExtentSheet    sheet;
        DxuiDpiScaler  scaler;
        SIZE           expected;
        SIZE           actual;



        sheet.AddExtentPage (kNarrowPx, kTallPagePx);
        sheet.AddExtentPage (kWidePx,   kShortPagePx);
        sheet.LayOut();

        scaler.SetDpi (kScrollTestDpi);
        expected = DxuiPropertySheet::ComputeMaxClientSizePx (SIZE {}, SIZE { kWidePx, kTallPagePx }, 0, scaler);
        actual   = sheet.GetMaxClientSizePx();

        Assert::AreEqual (expected.cx, actual.cx);
        Assert::AreEqual (expected.cy, actual.cy);
    }


    //  A request from the active page scrolls just far enough to show it.
    TEST_METHOD (PageScroll_RevealRequestBringsTheRectIntoView)
    {
        ScrollSheet  sheet;
        TallPage   * page   = sheet.AddPage (kTallPagePx);
        RECT         target = {};



        sheet.LayOut();
        target = MakeRect (0, sheet.GetAreaBottom() + kOverhangPx, kNarrowPx, sheet.GetAreaBottom() + kOverhangPx + kGrowthPx);

        page->RequestReveal (target);

        Assert::AreEqual ((int) (target.bottom - sheet.GetPageViewportPx().bottom), sheet.GetPageScrollPos());
    }


    //  Each dimension is held within its limits, and a maximum below the
    //  minimum gives way to it.
    TEST_METHOD (ClampSize_HoldsEachDimensionWithinItsLimits)
    {
        SIZE  low     = DxuiWindow::ClampSize (SIZE { 10, 10 },     SIZE { 100, 200 }, SIZE { 400, 500 });
        SIZE  high    = DxuiWindow::ClampSize (SIZE { 900, 900 },   SIZE { 100, 200 }, SIZE { 400, 500 });
        SIZE  inside  = DxuiWindow::ClampSize (SIZE { 300, 300 },   SIZE { 100, 200 }, SIZE { 400, 500 });
        SIZE  crossed = DxuiWindow::ClampSize (SIZE { 900, 900 },   SIZE { 100, 200 }, SIZE { 50,  60 });

        Assert::AreEqual (100L, low.cx);
        Assert::AreEqual (200L, low.cy);
        Assert::AreEqual (400L, high.cx);
        Assert::AreEqual (500L, high.cy);
        Assert::AreEqual (300L, inside.cx);
        Assert::AreEqual (300L, inside.cy);
        Assert::AreEqual (100L, crossed.cx);
        Assert::AreEqual (200L, crossed.cy);
    }


    //  A window opened smaller than its content grows to it, and one taller
    //  than the work area is cut to it and moved up to stay inside.
    TEST_METHOD (FitRectToMaxSize_GrowsToTheContentWithinTheWorkArea)
    {
        RECT  work   = { 0, 0, 2000, 1400 };
        RECT  grown  = DxuiWindow::FitRectToMaxSize (RECT { 100, 100, 820, 980 },  SIZE { 760, 1100 }, work);
        RECT  capped = DxuiWindow::FitRectToMaxSize (RECT { 100, 300, 820, 1180 }, SIZE { 760, 1600 }, work);

        Assert::AreEqual (100L,  grown.left);
        Assert::AreEqual (100L,  grown.top);
        Assert::AreEqual (860L,  grown.right);
        Assert::AreEqual (1200L, grown.bottom);
        Assert::AreEqual (100L,  capped.left);
        Assert::AreEqual (0L,    capped.top);
        Assert::AreEqual (860L,  capped.right);
        Assert::AreEqual (1400L, capped.bottom);
    }


    //  Content taller than the window grows it, kept inside the work area,
    //  and never narrows a window wider than the content; content that fits
    //  leaves the window alone.
    TEST_METHOD (TryGetGrownRect_GrowsOnlyWhereTheContentExceedsTheWindow)
    {
        RECT  work    = { 0, 0, 2000, 1400 };
        RECT  grown   = {};
        RECT  kept    = {};
        bool  isGrown = DxuiWindow::TryGetGrownRect (RECT { 100, 300, 1000, 1180 }, SIZE { 760, 1300 }, work, grown);
        bool  isKept  = DxuiWindow::TryGetGrownRect (RECT { 100, 100, 1000, 1180 }, SIZE { 760, 900 },  work, kept);

        Assert::IsTrue   (isGrown);
        Assert::AreEqual (100L,  grown.left);
        Assert::AreEqual (100L,  grown.top);
        Assert::AreEqual (1000L, grown.right);
        Assert::AreEqual (1400L, grown.bottom);
        Assert::IsFalse  (isKept);
    }
};





////////////////////////////////////////////////////////////////////////////////
//
//  TEST_CLASS
//
//
//   Covers the shared button-row ordering + left/right anchoring used by both
//   DxuiPropertySheet and DxuiDialogWindow so every command row lands in the
//   canonical Win32 order with secondary actions pinned bottom-left.
//
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiButtonRowTests)
{
public:
    TEST_METHOD (StandardRank_OrdersOkCancelApplyHelp)
    {
        using namespace DxuiButtonRow;

        Assert::IsTrue (GetStandardRank (IDOK)            < GetStandardRank (IDCANCEL));
        Assert::IsTrue (GetStandardRank (IDCANCEL)        < GetStandardRank (kApplyCommandId));
        Assert::IsTrue (GetStandardRank (kApplyCommandId) < GetStandardRank (IDHELP));
        Assert::IsTrue (GetStandardRank (IDYES)           < GetStandardRank (IDNO));
        Assert::IsTrue (GetStandardRank (IDNO)            < GetStandardRank (IDCANCEL));
    }


    TEST_METHOD (StandardRank_SyntheticIdSitsBetweenAffirmativeAndCancel)
    {
        using namespace DxuiButtonRow;

        // A dialog's synthetic result-code command id (not a standard IDxxx)
        // ranks after the affirmative buttons but before Cancel, so a stable
        // sort keeps author order for the primary actions.
        Assert::IsTrue (GetStandardRank (IDOK)   < GetStandardRank (12345));
        Assert::IsTrue (GetStandardRank (12345)  < GetStandardRank (IDCANCEL));
    }


    TEST_METHOD (LayoutLeftGroup_LeftAlignedFromEdge)
    {
        DxuiDpiScaler        scaler;
        RECT                 bounds    = MakeRect (0, 0, 400, 300);
        int                  widths[1] = { 75 };
        std::array<RECT, 1>  rects     = {};

        scaler.SetDpi (96);
        DxuiButtonRow::LayoutLeftGroup (bounds, scaler, widths, rects);

        // Edge pad 16, button 75x28, bottom margin 16 -> y = 300-16-28 = 256.
        AssertRect (MakeRect (16, 256, 91, 284), rects[0], L"browse");
        Assert::AreEqual (bounds.left + 16, rects[0].left);
    }

    TEST_METHOD (LayoutTabRects_WidthIsLabelPlusPadOnBothSides)
    {
        DxuiDpiScaler                scaler;
        RECT                         strip   = MakeRect (0, 0, 720, 36);
        std::array<DxuiSheetTab, 2>  tabs    = { DxuiSheetTab { 50.0f, true }, DxuiSheetTab { 80.0f, true } };
        std::array<RECT, 2>          rects   = {};

        scaler.SetDpi (96);
        DxuiPropertySheet::LayoutTabRects (strip, scaler, tabs, rects);

        // Left pad 16; widths 50+12*2 = 74 and 80+12*2 = 104, edge to edge.
        AssertRect (MakeRect (16, 0, 90,  36), rects[0], L"first");
        AssertRect (MakeRect (90, 0, 194, 36), rects[1], L"second");
    }


    TEST_METHOD (LayoutTabRects_ShortLabelGetsMinimumWidth)
    {
        DxuiDpiScaler                scaler;
        RECT                         strip   = MakeRect (0, 0, 720, 36);
        std::array<DxuiSheetTab, 2>  tabs    = { DxuiSheetTab { 20.0f, true }, DxuiSheetTab { 50.0f, true } };
        std::array<RECT, 2>          rects   = {};

        scaler.SetDpi (96);
        DxuiPropertySheet::LayoutTabRects (strip, scaler, tabs, rects);

        Assert::AreEqual ((LONG) DxuiPropertySheet::kTabMinWidthDip, rects[0].right - rects[0].left, L"held to the minimum");
        Assert::AreEqual (rects[0].right, rects[1].left, L"next tab starts where the short one ends");
    }


    TEST_METHOD (LayoutTabRects_HiddenTabTakesNoSpace)
    {
        DxuiDpiScaler                scaler;
        RECT                         strip   = MakeRect (0, 0, 720, 36);
        std::array<DxuiSheetTab, 3>  tabs    = { DxuiSheetTab { 50.0f, true },
                                                 DxuiSheetTab { 90.0f, false },
                                                 DxuiSheetTab { 50.0f, true } };
        std::array<RECT, 3>          rects   = {};

        scaler.SetDpi (96);
        DxuiPropertySheet::LayoutTabRects (strip, scaler, tabs, rects);

        AssertRect (MakeRect (16, 0, 90,  36), rects[0], L"first");
        AssertRect (MakeRect (0,  0, 0,   0),  rects[1], L"hidden");
        AssertRect (MakeRect (90, 0, 164, 36), rects[2], L"third follows the first");
    }


    TEST_METHOD (LayoutTabRects_ScalesWithDpi)
    {
        DxuiDpiScaler                scaler;
        RECT                         strip   = MakeRect (10, 5, 1090, 59);
        std::array<DxuiSheetTab, 2>  tabs    = { DxuiSheetTab { 50.0f, true }, DxuiSheetTab { 10.0f, true } };
        std::array<RECT, 2>          rects   = {};

        scaler.SetDpi (144);
        DxuiPropertySheet::LayoutTabRects (strip, scaler, tabs, rects);

        // Pad 24; 75 + 18*2 = 111; the short one is held to 64*1.5 = 96.
        AssertRect (MakeRect (34,  5, 145, 59), rects[0], L"first");
        AssertRect (MakeRect (145, 5, 241, 59), rects[1], L"second");
    }


    TEST_METHOD (MeasureTabLabelDip_ReturnsMeasuredWidth)
    {
        MockDxuiTextRenderer  text;
        float                 widthDip = 0.0f;
        HRESULT               hr       = S_OK;

        text.SetCannedMetrics (L"General", SIZE { 47, 17 });
        hr = DxuiPropertySheet::MeasureTabLabelDip (text, L"General", widthDip);

        Assert::AreEqual (S_OK, hr);
        Assert::AreEqual (47.0f, widthDip);
    }
};
