#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"
#include "Core/UnicodeSymbols.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPopupMenuTests
//
//  The one menu, driven headlessly through its plain-coordinate handlers with
//  no popup host, so everything below is the in-window path and deterministic.
//
//  Layout is checked from both anchors with one list, since the whole point
//  of the widget is that a title, a button and a right-click produce the same
//  rows. Width is checked against the mock's fixed glyph width, so the
//  content-fit rule, the accelerator column and the conditional check gutter
//  each have an exact expected number rather than an inequality.
//
//  Navigation covers the two things that go wrong in menus: a highlight that
//  lands on a separator or a disabled row, and a submenu that closes its
//  parent along with itself.
//
//  The reopen guard runs on an injected clock, so the close window is crossed
//  by arithmetic rather than by sleeping.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiPopupMenuTests)
{
public:

    static constexpr int  s_kGlyphPx = 7;         // MockDxuiTextRenderer's fallback

    RECT  MakeHost (int w, int h) { return RECT { 0, 0, w, h }; }

    //
    //  Geometry read from the widget's own metrics rather than repeated here
    //  as numbers. The metrics come from the Windows menu settings, so a
    //  hard-coded row height would assert about the display the suite happens
    //  to run on; the column model these check is the same at every DPI.
    //
    static int  RowPx      (const DxuiPopupMenu & m) { return m.GetMetrics().rowHeightPx;       }
    static int  SepPx      (const DxuiPopupMenu & m) { return m.GetMetrics().separatorHeightPx; }
    static int  MinWidthPx (const DxuiPopupMenu & m) { return m.GetMetrics().minWidthPx;        }
    static int  AccelGapPx (const DxuiPopupMenu & m) { return m.GetMetrics().accelGapPx;        }

    //  The label column's left edge, and everything to the right of the
    //  accelerator column, which together are the menu's fixed width cost.
    static int  LabelLeftPx (const DxuiPopupMenu & m)
    {
        return m.GetMetrics().leftPadPx + m.GetMetrics().checkGutterPx + m.GetMetrics().gutterGapPx;
    }

    static int  RightPadPx  (const DxuiPopupMenu & m) { return m.GetMetrics().rightPadPx; }


    //
    //  Five commands the tests share: a long label so the menu clears its
    //  width floor and the arithmetic is visible; a separator; a disabled row;
    //  a plain row; and a submenu with a disabled first child.
    //
    struct Fixture
    {
        DxuiCommand  alpha;
        DxuiCommand  beta;
        DxuiCommand  gamma;
        DxuiCommand  sub;
        DxuiCommand  childA;
        DxuiCommand  childB;
        int          dispatched     = 0;
        int          lastDispatched = -1;

        Fixture()
        {
            alpha.id    = 1;
            alpha.label = L"A very long menu label here";      // 27 glyphs
            alpha.dispatch = [this] () { dispatched++; lastDispatched = 1; };

            beta.id    = 2;
            beta.label = L"Beta";
            beta.isEnabled = [] () { return false; };
            beta.dispatch  = [this] () { dispatched++; lastDispatched = 2; };

            gamma.id    = 3;
            gamma.label = L"Gamma";
            gamma.dispatch = [this] () { dispatched++; lastDispatched = 3; };

            sub.id    = 4;
            sub.label = L"More";
            sub.dispatch = [] () {};

            childA.id    = 5;
            childA.label = L"Child A";
            childA.isEnabled = [] () { return false; };
            childA.dispatch  = [this] () { dispatched++; lastDispatched = 5; };

            childB.id    = 6;
            childB.label = L"Child B";
            childB.dispatch = [this] () { dispatched++; lastDispatched = 6; };
        }

        //  alpha, ---, beta(disabled), gamma
        std::vector<DxuiPopupMenuItem>  FlatList()
        {
            std::vector<DxuiPopupMenuItem>  rows;

            rows.push_back (DxuiPopupMenuItem::ForCommand (&alpha));
            rows.push_back (DxuiPopupMenuItem::ForSeparator());
            rows.push_back (DxuiPopupMenuItem::ForCommand (&beta));
            rows.push_back (DxuiPopupMenuItem::ForCommand (&gamma));
            return rows;
        }

        //  alpha, sub > [childA(disabled), childB]
        std::vector<DxuiPopupMenuItem>  NestedList()
        {
            std::vector<DxuiPopupMenuItem>  rows;
            std::vector<DxuiPopupMenuItem>  kids;

            kids.push_back (DxuiPopupMenuItem::ForCommand (&childA));
            kids.push_back (DxuiPopupMenuItem::ForCommand (&childB));

            rows.push_back (DxuiPopupMenuItem::ForCommand (&alpha));
            rows.push_back (DxuiPopupMenuItem::ForSubmenu (&sub, std::move (kids)));
            return rows;
        }
    };


    TEST_METHOD (ShowUnderAndShowAt_LayOutOneListIdentically)
    {
        Fixture               f;
        DxuiPopupMenu         under;
        DxuiPopupMenu         at;
        MockDxuiTextRenderer  text;
        RECT                  anchor = { 40, 0, 120, 32 };
        RECT                  ru     = {};
        RECT                  ra     = {};


        under.ShowUnder (anchor, f.FlatList(), text, MakeHost (800, 600));
        at.ShowAt (anchor.left, anchor.bottom, f.FlatList(), text, MakeHost (800, 600));

        ru = under.GetRect();
        ra = at.GetRect();

        Assert::IsTrue (under.IsVisible());
        Assert::IsTrue (at.IsVisible());
        Assert::AreEqual (ru.left,   ra.left);
        Assert::AreEqual (ru.top,    ra.top);
        Assert::AreEqual (ru.right,  ra.right);
        Assert::AreEqual (ru.bottom, ra.bottom);

        // Same row under the same point, including the separator gap.
        Assert::AreEqual (under.HitTest (ru.left + 5, ru.top + RowPx (under) + SepPx (under) + 5),
                          at.HitTest    (ra.left + 5, ra.top + RowPx (at)    + SepPx (at)    + 5));
    }


    TEST_METHOD (Height_IsRowsPlusSeparators)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        RECT                  r = {};


        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        r = menu.GetRect();

        Assert::AreEqual ((LONG) (3 * RowPx (menu) + SepPx (menu)), r.bottom - r.top);
    }


    TEST_METHOD (Width_IsLabelColumnPlusAcceleratorColumn)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        int                   labelPx = 27 * s_kGlyphPx;
        int                   fixedPx = 0;
        int                   bare    = 0;
        int                   withAcc = 0;
        int                   withChk = 0;
        uint64_t              now     = 0;


        // Each re-show below follows a hide at the same anchor, which is the
        // exact gesture the reopen guard exists to refuse. Step the clock
        // past its window between shows so the guard stays out of a width
        // test.
        menu.SetClock ([&] () { return now; });

        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        fixedPx = LabelLeftPx (menu) + RightPadPx (menu);
        bare    = menu.GetRect().right - menu.GetRect().left;
        Assert::AreEqual (fixedPx + labelPx, bare);
        Assert::IsTrue (bare > MinWidthPx (menu));

        // An accelerator anywhere opens the accelerator column: the gap plus
        // the widest accelerator, on top of the unchanged label column.
        f.alpha.accelerator = L"Ctrl+X";                        // 6 glyphs
        menu.Hide();
        now += 1000;
        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        withAcc = menu.GetRect().right - menu.GetRect().left;
        Assert::AreEqual (bare + AccelGapPx (menu) + 6 * s_kGlyphPx, withAcc);

        // The check gutter is reserved whether or not a row can check, so a
        // command that gains a checked state moves nothing.
        f.gamma.isChecked = [] () { return true; };
        menu.Hide();
        now += 1000;
        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        withChk = menu.GetRect().right - menu.GetRect().left;
        Assert::AreEqual (withAcc, withChk);
    }


    //
    //  The defect the two-column measurement exists for: sizing the menu to
    //  the widest label-plus-accelerator row leaves an accelerator-less row
    //  with a LONGER label free to run under the accelerators above it.
    //
    TEST_METHOD (Width_LongLabelWithoutAccelerator_ClearsTheAcceleratorColumn)
    {
        DxuiCommand                     shortWithAccel;
        DxuiCommand                     longNoAccel;
        DxuiPopupMenu                   menu;
        MockDxuiTextRenderer            text;
        std::vector<DxuiPopupMenuItem>  rows;
        int                             width = 0;


        shortWithAccel.label       = L"Eject drive 1";                                    // 13 glyphs
        shortWithAccel.accelerator = L"Ctrl+Shift+1";                                     // 12 glyphs
        shortWithAccel.dispatch    = [] () {};

        longNoAccel.label    = L"Set \"JoystickTest.dsk\" internal write-protect flag";    // 50 glyphs
        longNoAccel.dispatch = [] () {};

        rows.push_back (DxuiPopupMenuItem::ForCommand (&shortWithAccel));
        rows.push_back (DxuiPopupMenuItem::ForCommand (&longNoAccel));

        menu.ShowAt (0, 0, std::move (rows), text, MakeHost (1600, 600));
        width = menu.GetRect().right - menu.GetRect().left;

        // The long label sets the label column and the accelerator column
        // still sits entirely to its right.
        Assert::AreEqual (LabelLeftPx (menu) + 50 * s_kGlyphPx
                              + AccelGapPx (menu) + 12 * s_kGlyphPx + RightPadPx (menu),
                          width);
    }


    //
    //  What the column model is FOR, checked at the paint call rather than
    //  through the width: accelerators of unequal length share a left edge,
    //  are drawn left aligned, and every label is clipped short of them.
    //
    TEST_METHOD (Paint_AcceleratorsShareALeftEdge_AndLabelsStopShortOfThem)
    {
        DxuiCommand                     shortAccel;
        DxuiCommand                     longAccel;
        DxuiCommand                     longLabel;
        DxuiPopupMenu                   menu;
        MockDxuiPainter                 painter;
        MockDxuiTextRenderer            text;
        MockDxuiTheme                   theme;
        std::vector<DxuiPopupMenuItem>  rows;
        float                           accelX    = -1.0f;
        int                             accels    = 0;
        float                           labelEdge = 0.0f;


        shortAccel.label       = L"Insert drive 1...";
        shortAccel.accelerator = L"Ctrl+1";
        shortAccel.dispatch    = [] () {};

        longAccel.label       = L"Eject drive 1";
        longAccel.accelerator = L"Ctrl+Shift+1";
        longAccel.dispatch    = [] () {};

        longLabel.label    = L"Set \"JoystickTest.dsk\" internal write-protect flag";
        longLabel.dispatch = [] () {};

        rows.push_back (DxuiPopupMenuItem::ForCommand (&shortAccel));
        rows.push_back (DxuiPopupMenuItem::ForCommand (&longAccel));
        rows.push_back (DxuiPopupMenuItem::ForCommand (&longLabel));

        menu.SetTheme (&theme);
        menu.ShowAt (0, 0, std::move (rows), text, MakeHost (1600, 600));
        menu.Paint (painter, text);

        for (const RecordedTextCall & c : text.Calls())
        {
            if (c.kind != RecordedTextKind::DrawString)
            {
                continue;
            }

            if (c.text == L"Ctrl+1" || c.text == L"Ctrl+Shift+1")
            {
                Assert::AreEqual ((int) DxuiTextHAlign::Left, (int) c.hAlign);

                if (accelX < 0.0f)
                {
                    accelX = c.x;
                }

                Assert::AreEqual (accelX, c.x);
                accels++;
            }
            else
            {
                labelEdge = c.x + c.width;
            }
        }

        Assert::AreEqual (2, accels);
        Assert::IsTrue (accelX > 0.0f);

        // The last label drawn is the long one; its box ends at the
        // accelerator column rather than running under it.
        Assert::AreEqual (accelX, labelEdge);
    }


    TEST_METHOD (Width_NeverBelowFloor)
    {
        DxuiCommand                     tiny;
        DxuiPopupMenu                   menu;
        MockDxuiTextRenderer            text;
        std::vector<DxuiPopupMenuItem>  rows;


        tiny.label    = L"Hi";
        tiny.dispatch = [] () {};
        rows.push_back (DxuiPopupMenuItem::ForCommand (&tiny));

        menu.ShowAt (0, 0, std::move (rows), text, MakeHost (800, 600));

        Assert::AreEqual ((LONG) MinWidthPx (menu), menu.GetRect().right - menu.GetRect().left);
    }


    TEST_METHOD (ShowAt_NearClientEdge_IsClampedInside)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        RECT                  host = MakeHost (400, 300);
        RECT                  r    = {};


        menu.ShowAt (390, 290, f.FlatList(), text, host);
        r = menu.GetRect();

        Assert::IsTrue (r.left   >= host.left);
        Assert::IsTrue (r.top    >= host.top);
        Assert::IsTrue (r.right  <= host.right);
        Assert::IsTrue (r.bottom <= host.bottom);
    }


    TEST_METHOD (Down_SkipsSeparatorAndDisabled_AndWraps)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;


        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        Assert::AreEqual (-1, menu.GetHighlight());

        Assert::IsTrue   (menu.OnKey (VK_DOWN));
        Assert::AreEqual (0, menu.GetHighlight());     // alpha
        Assert::IsTrue   (menu.OnKey (VK_DOWN));
        Assert::AreEqual (3, menu.GetHighlight());     // gamma: over the separator and disabled beta
        Assert::IsTrue   (menu.OnKey (VK_DOWN));
        Assert::AreEqual (0, menu.GetHighlight());     // wrap

        Assert::IsTrue   (menu.OnKey (VK_UP));
        Assert::AreEqual (3, menu.GetHighlight());     // wrap backwards
    }


    TEST_METHOD (Right_OpensSubmenu_WithFirstEnabledRowHighlighted)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;


        menu.ShowAt (0, 0, f.NestedList(), text, MakeHost (800, 600));
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_DOWN);
        Assert::AreEqual (1, menu.GetHighlight());
        Assert::IsFalse  (menu.HasOpenChild());

        Assert::IsTrue   (menu.OnKey (VK_RIGHT));
        Assert::IsTrue   (menu.HasOpenChild());
        Assert::IsNotNull (menu.GetChild());
        Assert::AreEqual (1, menu.GetChild()->GetHighlight());    // childB; childA is disabled
    }


    TEST_METHOD (HoverOnSubmenuRow_OpensUnhighlighted_HoverElsewhereCloses)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        RECT                  r = {};


        // Zero delay is the open-on-contact rule; the dwell is covered
        // separately below.
        menu.SetSubmenuDelayMs (0);
        menu.ShowAt (0, 0, f.NestedList(), text, MakeHost (800, 600));
        r = menu.GetRect();

        menu.OnMouseMove (r.left + 5, r.top + RowPx (menu) + 5);    // the submenu row
        Assert::IsTrue   (menu.HasOpenChild());
        Assert::AreEqual (-1, menu.GetChild()->GetHighlight());

        menu.OnMouseMove (r.left + 5, r.top + 5);                   // back to alpha
        Assert::IsFalse  (menu.HasOpenChild());
        Assert::IsTrue   (menu.IsVisible());
    }


    //
    //  A submenu opens after the pointer RESTS on its row, which is what
    //  lets a diagonal move to a row below cross a submenu row without
    //  opening it. The clock is injected, so the delay is crossed by
    //  arithmetic rather than by sleeping.
    //
    TEST_METHOD (HoverOnSubmenuRow_WaitsOutTheDelay_AndLeavingCancelsIt)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        RECT                  r     = {};
        uint64_t              now   = 1000;
        const int             delay = 400;


        menu.SetClock ([&] () { return now; });
        menu.SetSubmenuDelayMs (delay);
        menu.ShowAt (0, 0, f.NestedList(), text, MakeHost (800, 600));
        r = menu.GetRect();

        // Crossing the row arms the open; it does not perform it.
        menu.OnMouseMove (r.left + 5, r.top + RowPx (menu) + 5);
        Assert::IsFalse (menu.HasOpenChild());
        Assert::IsTrue  (menu.WantsTick());

        // Ticking before the delay is up changes nothing.
        now += delay - 1;
        menu.Tick ((int64_t) now);
        Assert::IsFalse (menu.HasOpenChild());

        now += 1;
        menu.Tick ((int64_t) now);
        Assert::IsTrue  (menu.HasOpenChild());
        Assert::IsFalse (menu.WantsTick());

        // Leaving the row closes the child, and the pointer passing back
        // across it arms rather than reopens.
        menu.OnMouseMove (r.left + 5, r.top + 5);
        Assert::IsFalse (menu.HasOpenChild());

        menu.OnMouseMove (r.left + 5, r.top + RowPx (menu) + 5);
        Assert::IsFalse (menu.HasOpenChild());

        // ... and moving away again before the delay cancels it outright:
        // no amount of ticking opens a submenu the pointer has left.
        menu.OnMouseMove (r.left + 5, r.top + 5);
        now += delay * 2;
        menu.Tick ((int64_t) now);
        Assert::IsFalse (menu.HasOpenChild());
        Assert::IsFalse (menu.WantsTick());
    }


    //
    //  Keyboard and click open a submenu OUTRIGHT. The dwell exists for a
    //  pointer crossing rows it did not mean to open; a deliberate Right or
    //  a click is not that.
    //
    TEST_METHOD (RightArrow_OpensSubmenuWithoutWaitingOutTheDelay)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        uint64_t              now = 1000;


        menu.SetClock ([&] () { return now; });
        menu.SetSubmenuDelayMs (400);
        menu.ShowAt (0, 0, f.NestedList(), text, MakeHost (800, 600));

        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_DOWN);
        Assert::IsTrue (menu.OnKey (VK_RIGHT));
        Assert::IsTrue (menu.HasOpenChild());
    }


    TEST_METHOD (Left_ClosesOnlyTheChild)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;


        menu.ShowAt (0, 0, f.NestedList(), text, MakeHost (800, 600));
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_RIGHT);
        Assert::IsTrue (menu.HasOpenChild());

        Assert::IsTrue  (menu.OnKey (VK_LEFT));
        Assert::IsFalse (menu.HasOpenChild());
        Assert::IsTrue  (menu.IsVisible());
        Assert::AreEqual (1, menu.GetHighlight());
    }


    TEST_METHOD (Escape_WithChildOpen_ClosesOnlyTheChild_ThenTheRoot)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        int                   closedCount   = 0;
        bool                  lastCommitted = true;


        menu.SetOnClosed ([&] (bool committed) { closedCount++; lastCommitted = committed; });
        menu.ShowAt (0, 0, f.NestedList(), text, MakeHost (800, 600));
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_RIGHT);

        menu.OnKey (VK_ESCAPE);
        Assert::IsFalse  (menu.HasOpenChild());
        Assert::IsTrue   (menu.IsVisible());
        Assert::AreEqual (0, closedCount);       // the root's callback is the root's only

        menu.OnKey (VK_ESCAPE);
        Assert::IsFalse  (menu.IsVisible());
        Assert::AreEqual (1, closedCount);
        Assert::IsFalse  (lastCommitted);
    }


    TEST_METHOD (Enter_CommitsAndDispatches_ClosedFiresBeforeSelect)
    {
        Fixture                    f;
        DxuiPopupMenu              menu;
        MockDxuiTextRenderer       text;
        std::vector<std::wstring>  order;
        int                        selected = -1;


        menu.SetOnClosed ([&] (bool committed) { order.push_back (committed ? L"closed:true" : L"closed:false"); });
        menu.SetOnSelect ([&] (int idx) { order.push_back (L"select"); selected = idx; });

        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_DOWN);                    // gamma
        Assert::IsTrue (menu.OnKey (VK_RETURN));

        Assert::IsFalse  (menu.IsVisible());
        Assert::AreEqual ((size_t) 2, order.size());
        Assert::AreEqual (std::wstring (L"closed:true"), order[0]);
        Assert::AreEqual (std::wstring (L"select"),      order[1]);
        Assert::AreEqual (3, selected);
        Assert::AreEqual (1, f.dispatched);
        Assert::AreEqual (3, f.lastDispatched);
    }


    TEST_METHOD (Enter_InSubmenu_DispatchesChildAndClosesWholeChain)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        int                   closedCount = 0;


        menu.SetOnClosed ([&] (bool) { closedCount++; });
        menu.ShowAt (0, 0, f.NestedList(), text, MakeHost (800, 600));
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_RIGHT);                    // child open, childB highlighted
        menu.OnKey (VK_RETURN);

        Assert::AreEqual (6, f.lastDispatched);
        Assert::IsFalse  (menu.IsVisible());
        Assert::IsFalse  (menu.HasOpenChild());
        Assert::AreEqual (1, closedCount);
    }


    TEST_METHOD (DisabledCommand_DoesNotDispatchOnEnter)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        RECT                  r = {};


        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        r = menu.GetRect();

        // Hover CAN rest on a disabled row, as it does in every platform menu.
        menu.OnMouseMove (r.left + 5, r.top + RowPx (menu) + SepPx (menu) + 5);      // beta
        Assert::AreEqual (2, menu.GetHighlight());

        Assert::IsTrue   (menu.OnKey (VK_RETURN));
        Assert::AreEqual (0, f.dispatched);
        Assert::IsTrue   (menu.IsVisible());
    }


    TEST_METHOD (ClickRelease_OnSameRow_Commits_DragOffCancels)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        RECT                  r = {};


        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        r = menu.GetRect();

        Assert::IsTrue (menu.OnLButtonDown (r.left + 5, r.top + 5));
        Assert::IsTrue (menu.OnLButtonUp   (r.left + 5, r.top + 3 * RowPx (menu)));   // released over gamma
        Assert::AreEqual (0, f.dispatched);
        Assert::IsTrue   (menu.IsVisible());

        Assert::IsTrue (menu.OnLButtonDown (r.left + 5, r.top + 5));
        Assert::IsTrue (menu.OnLButtonUp   (r.left + 5, r.top + 5));
        Assert::AreEqual (1, f.lastDispatched);
        Assert::IsFalse  (menu.IsVisible());
    }


    TEST_METHOD (ReopenGuard_SwallowsShowInsideCloseWindow_SameAnchorOnly)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        uint64_t              now    = 10000;
        RECT                  anchor = { 0, 0, 50, 32 };
        RECT                  other  = { 200, 0, 250, 32 };


        menu.SetClock ([&] () { return now; });

        menu.ShowUnder (anchor, f.FlatList(), text, MakeHost (800, 600));
        Assert::IsTrue (menu.IsVisible());
        menu.Hide();

        now += 100;
        menu.ShowUnder (anchor, f.FlatList(), text, MakeHost (800, 600));
        Assert::IsFalse (menu.IsVisible());                   // inside the window, same anchor

        menu.ShowUnder (other, f.FlatList(), text, MakeHost (800, 600));
        Assert::IsTrue (menu.IsVisible());                    // different anchor is not a toggle
        menu.Hide();

        now += 300;
        menu.ShowUnder (other, f.FlatList(), text, MakeHost (800, 600));
        Assert::IsTrue (menu.IsVisible());                    // window elapsed
    }


    TEST_METHOD (Paint_ReadsCheckedAndLabelFromCommandAtPaintTime)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        bool                  checked   = false;
        std::wstring          label     = L"First";
        bool  sawCheck   = false;
        bool  sawLabel   = false;
        bool  sawDivider = false;


        f.gamma.isChecked = [&] () { return checked; };
        f.gamma.labelText = [&] () { return label; };

        menu.SetTheme (&theme);
        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));

        menu.Paint (painter, text);
        for (const RecordedTextCall & c : text.Calls())
        {
            if (c.kind == RecordedTextKind::DrawString && c.text == s_kpszCheckMark) { sawCheck = true; }
            if (c.kind == RecordedTextKind::DrawString && c.text == L"First")  { sawLabel = true; }
        }

        for (const RecordedPaintCall & p : painter.Calls())
        {
            if (p.kind == RecordedPaintKind::FillRect && p.argb == MockDxuiTheme::s_kDivider && p.height <= 2.0f)
            {
                sawDivider = true;
            }
        }

        Assert::IsFalse (sawCheck);
        Assert::IsTrue  (sawLabel);
        Assert::IsTrue  (sawDivider);

        checked = true;
        label   = L"Second";
        text.Reset();
        menu.Paint (painter, text);
        sawCheck = false;
        sawLabel = false;
        for (const RecordedTextCall & c : text.Calls())
        {
            if (c.kind == RecordedTextKind::DrawString && c.text == s_kpszCheckMark)  { sawCheck = true; }
            if (c.kind == RecordedTextKind::DrawString && c.text == L"Second")  { sawLabel = true; }
        }

        Assert::IsTrue (sawCheck);
        Assert::IsTrue (sawLabel);
    }


    TEST_METHOD (Paint_DisabledRowUsesDisabledColor)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        uint32_t              betaArgb  = 0;
        uint32_t              gammaArgb = 0;


        menu.SetTheme (&theme);
        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        menu.Paint (painter, text);

        for (const RecordedTextCall & c : text.Calls())
        {
            if (c.kind != RecordedTextKind::DrawString) { continue; }
            if (c.text == L"Beta")  { betaArgb  = c.argb; }
            if (c.text == L"Gamma") { gammaArgb = c.argb; }
        }

        Assert::AreEqual (MockDxuiTheme::s_kForegroundDisabled, betaArgb);
        Assert::AreEqual (MockDxuiTheme::s_kForeground,         gammaArgb);
    }


    TEST_METHOD (LegacyItems_BecomeCheckableRows_AndSelectReportsIndex)
    {
        DxuiPopupMenu                    menu;
        MockDxuiPainter                  painter;
        MockDxuiTextRenderer             text;
        MockDxuiTheme                    theme;
        std::vector<DxuiPopupMenu::Item> items;
        int                              selected = -1;
        int                              checks   = 0;
        int                              width    = 0;


        items.push_back (DxuiPopupMenu::Item { L"Color", false });
        items.push_back (DxuiPopupMenu::Item { L"Green", true  });
        items.push_back (DxuiPopupMenu::Item { L"Amber", false });

        menu.SetTheme (&theme);
        menu.SetOnSelect ([&] (int idx) { selected = idx; });
        menu.Show (0, 0, items, text, MakeHost (800, 600));

        Assert::AreEqual ((size_t) 3, menu.GetItems().size());
        Assert::AreEqual ((size_t) 3, menu.GetRows().size());

        // Every legacy row is checkable, so the list keeps its gutter as the
        // old widget always reserved it.
        width = menu.GetRect().right - menu.GetRect().left;
        Assert::AreEqual (MinWidthPx (menu), width);      // short labels, floor applies

        menu.Paint (painter, text);
        for (const RecordedTextCall & c : text.Calls())
        {
            if (c.kind == RecordedTextKind::DrawString && c.text == s_kpszCheckMark) { checks++; }
        }

        Assert::AreEqual (1, checks);

        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_DOWN);
        menu.OnKey (VK_DOWN);                         // Amber
        menu.OnKey (VK_RETURN);
        Assert::AreEqual (2, selected);
        Assert::IsFalse  (menu.IsVisible());
    }


    TEST_METHOD (HighlightChange_FiresOnPointerAndKeys)
    {
        Fixture               f;
        DxuiPopupMenu         menu;
        MockDxuiTextRenderer  text;
        std::vector<int>      seen;
        RECT                  r = {};


        menu.SetOnHighlightChange ([&] (int idx) { seen.push_back (idx); });
        menu.ShowAt (0, 0, f.FlatList(), text, MakeHost (800, 600));
        r = menu.GetRect();

        menu.OnKey (VK_DOWN);                                      // 0
        menu.OnMouseMove (r.left + 5, r.top + 2 * RowPx (menu) + SepPx (menu) + 5);   // 3
        menu.OnMouseMove (r.left + 6, r.top + 2 * RowPx (menu) + SepPx (menu) + 6);   // same row: no fire

        Assert::AreEqual ((size_t) 2, seen.size());
        Assert::AreEqual (0, seen[0]);
        Assert::AreEqual (3, seen[1]);
    }
};
