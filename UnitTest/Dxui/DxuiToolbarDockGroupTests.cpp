#include "Pch.h"

#include "MockDxuiControl.h"
#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  BandTestEntry
//
//  A custom entry of a set length, standing in for the history timeline's
//  strip of pictures and for a toolbar's buttons.
//
////////////////////////////////////////////////////////////////////////////////

class BandTestEntry : public IDxuiToolbarCustomEntry
{
public:
    explicit BandTestEntry (int widthPx) : m_widthPx (widthPx) {}

    int              GetWidthPx    (bool, const DxuiDpiScaler &, IDxuiTextRenderer *) const override { return m_widthPx; }
    int              GetMinWidthPx (const DxuiDpiScaler &) const                             override { return m_widthPx; }
    void             Layout        (const RECT &, bool, const DxuiDpiScaler &)              override {}
    void             Paint         (IDxuiPainter &, IDxuiTextRenderer &, const IDxuiTheme &, bool, bool, bool) override {}
    const wchar_t *  GetTooltipAt  (int, int, RECT &) const                                 override { return nullptr; }
    bool             OnClick       (int, int)                                               override { return false; }

private:
    int  m_widthPx = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiToolbarDockGroupTests
//
//  Two dockable toolbars of one window laid out together, as the debugger's
//  history timeline and command bar are: sharing a band, where the timeline
//  takes what the command bar leaves, or each in a band of its own; carried
//  from one band to another or to a new band between them; and saved with
//  their bands, places saved before bands existed loading as they stood.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DxuiToolbarDockGroupTests)
{
public:

    using  Edge = DxuiToolbarDock::Edge;

    static constexpr RECT  kArea      = { 0, 100, 1000, 800 };
    static constexpr int   kMargin    = DxuiToolbarHost::kMarginDp;
    static constexpr int   kStripPx   = 300;
    static constexpr int   kButtonsPx = 150;
    static constexpr int   kLabelRoom = 40;


    struct Rig
    {
        DxuiToolbarDockGroup  group;
        DxuiToolbar           stripBar;
        DxuiToolbar           toolBar;
        BandTestEntry         stripEntry { kStripPx };
        BandTestEntry         toolEntry  { kButtonsPx };
        DxuiToolbarHost       strip;
        DxuiToolbarHost       tool;
        DxuiDpiScaler         scaler;
        RECT                  area       = kArea;
        RECT                  inner      = {};
        std::wstring          stripSaved;
        std::wstring          toolSaved;

        Rig (const wchar_t * stripText, const wchar_t * toolText, DxuiDockSite * site = nullptr)
        {
            scaler.SetDpi (USER_DEFAULT_SCREEN_DPI);

            SetUpBar (stripBar, stripEntry, true);
            SetUpBar (toolBar,  toolEntry,  false);

            stripBar.SetBandDp (DxuiToolbar::GetBandDip() + kLabelRoom);

            strip.Attach       (nullptr, &stripBar, nullptr, nullptr);
            strip.SetFillsEdge (true);
            tool.Attach        (nullptr, &toolBar, site, nullptr);

            //  The timeline joins first, as it stood against the edge before
            //  bands existed.
            strip.JoinGroup (group);
            tool.JoinGroup  (group);

            strip.SetDock (DxuiToolbarDock::FromText (stripText));
            tool.SetDock  (DxuiToolbarDock::FromText (toolText));

            strip.SetAnimationsEnabled (false);
            tool.SetAnimationsEnabled  (false);

            strip.SetOnSave   ([this] (const std::wstring & text) { stripSaved = text; });
            tool.SetOnSave    ([this] (const std::wstring & text) { toolSaved  = text; });
            strip.SetOnLayout ([this] { Lay(); });
            tool.SetOnLayout  ([this] { Lay(); });

            Lay();
        }

        void Lay() { inner = group.Layout (area, area, scaler); }

        int  GetToolLength() { return toolBar.GetNaturalLengthPx (scaler); }
        int  GetThin() const { return DxuiToolbar::GetBandDip(); }
        int  GetTall() const { return DxuiToolbar::GetBandDip() + kLabelRoom; }

        //  A press on a toolbar's grab handle, then moves to each point.
        void Carry (DxuiToolbarHost & host, DxuiToolbar & bar, std::initializer_list<POINT> points)
        {
            DxuiMouseEvent  ev;
            RECT            grip = bar.GetGripRect();

            ev.kind        = DxuiMouseEventKind::Down;
            ev.button      = DxuiMouseButton::Left;
            ev.positionDip = POINT { (grip.left + grip.right) / 2, (grip.top + grip.bottom) / 2 };

            Assert::IsTrue (host.RouteDrag (ev), L"the handle takes the press");

            for (POINT point : points)
            {
                ev.kind        = DxuiMouseEventKind::Move;
                ev.positionDip = point;
                host.RouteDrag (ev);
            }
        }

        void Release (DxuiToolbarHost & host)
        {
            DxuiMouseEvent  ev;

            ev.kind   = DxuiMouseEventKind::Up;
            ev.button = DxuiMouseButton::Left;
            host.RouteDrag (ev);
        }
    };


    static void SetUpBar (DxuiToolbar & bar, BandTestEntry & custom, bool fill)
    {
        std::vector<DxuiToolbar::Entry>  entries (1);
        auto                             command = std::make_shared<DxuiCommand>();

        command->id    = 1;
        command->label = L"Entry";

        entries[0].command       = command;
        entries[0].custom        = &custom;
        entries[0].neverOverflow = true;
        entries[0].fill          = fill;

        bar.SetGrabHandle (true);
        bar.SetEntries    (std::move (entries));
    }


    static void AssertRect (const RECT & expected, const RECT & actual, const wchar_t * msg)
    {
        std::wstring  text = std::format (L"{} expected {{{}, {}, {}, {}}} actual {{{}, {}, {}, {}}}", msg,
                                          expected.left, expected.top, expected.right, expected.bottom,
                                          actual.left, actual.top, actual.right, actual.bottom);

        Assert::IsTrue (EqualRect (&expected, &actual) != FALSE, text.c_str());
    }


    //  The saved places, loaded into a new window, lay out as they were.
    static void AssertReloadsAsSaved (const Rig & saved)
    {
        Rig  reloaded (saved.stripSaved.c_str(), saved.toolSaved.c_str());

        AssertRect (saved.stripBar.GetBounds(), reloaded.stripBar.GetBounds(), L"the strip comes back where it was");
        AssertRect (saved.toolBar.GetBounds(),  reloaded.toolBar.GetBounds(),  L"and the toolbar");
    }


    TEST_METHOD (TheStripInTheToolbarsBandFillsWhatTheToolbarLeaves)
    {
        Rig   rig (L"top 500 band 0", L"top 0 band 0");
        int   len  = rig.GetToolLength();
        int   tall = rig.GetTall();
        int   thin = rig.GetThin();

        AssertRect (RECT { kMargin, 100 + (tall - thin) / 2, kMargin + len, 100 + (tall - thin) / 2 + thin }, rig.toolBar.GetBounds(),
                    L"the toolbar keeps its natural length, centered across the band");
        AssertRect (RECT { kMargin + len, 100, 1000 - kMargin, 100 + tall }, rig.stripBar.GetBounds(), L"the strip takes the rest of the band");
        AssertRect (RECT { 0, 100 + tall, 1000, 800 }, rig.inner, L"one band comes out of the area");

        rig.area = RECT { 0, 100, 1400, 800 };
        rig.Lay();

        Assert::AreEqual (1400L - kMargin, rig.stripBar.GetBounds().right, L"the strip follows the window's width");
        Assert::AreEqual ((long) len, rig.toolBar.GetBounds().right - rig.toolBar.GetBounds().left, L"the toolbar does not");
    }


    TEST_METHOD (TheStripCanStandBeforeTheToolbar)
    {
        Rig   rig (L"bottom 0 band 0", L"bottom 600 band 0");
        int   len = rig.GetToolLength();

        Assert::AreEqual ((long) kMargin,            rig.stripBar.GetBounds().left, L"the strip first");
        Assert::AreEqual (rig.stripBar.GetBounds().right, rig.toolBar.GetBounds().left);
        Assert::AreEqual (1000L - kMargin,           rig.toolBar.GetBounds().right, L"the toolbar at the band's far end");
        Assert::AreEqual ((long) len, rig.toolBar.GetBounds().right - rig.toolBar.GetBounds().left);
    }


    TEST_METHOD (TheStripInABandOfItsOwnTakesTheWholeRowOnEveryEdge)
    {
        for (const wchar_t * edge : { L"top", L"bottom", L"left", L"right" })
        {
            for (int stripBand : { 0, 1 })
            {
                std::wstring  stripText = std::format (L"{} 300 band {}", edge, stripBand);
                std::wstring  toolText  = std::format (L"{} 50 band {}",  edge, 1 - stripBand);
                Rig           rig (stripText.c_str(), toolText.c_str());
                RECT          s         = rig.stripBar.GetBounds();
                RECT          t         = rig.toolBar.GetBounds();
                bool          side      = rig.strip.GetDock().IsVertical();
                long          depth     = rig.GetTall() + rig.GetThin();

                if (side)
                {
                    Assert::AreEqual (100L, s.top,    stripText.c_str());
                    Assert::AreEqual (800L, s.bottom, L"the strip runs the whole height");
                    Assert::AreEqual (150L, t.top,    L"the toolbar keeps its offset");
                }
                else
                {
                    Assert::AreEqual ((long) kMargin,  s.left,  stripText.c_str());
                    Assert::AreEqual (1000L - kMargin, s.right, L"the strip runs the whole width");
                    Assert::AreEqual (58L,             t.left,  L"the toolbar keeps its offset");
                }

                Assert::AreEqual (1000L * 700L - (side ? depth * 700L : depth * 1000L),
                                  (rig.inner.right - rig.inner.left) * (rig.inner.bottom - rig.inner.top), L"both bands come out of the area");
            }
        }
    }


    TEST_METHOD (TheOuterBandIsTheOneAgainstTheEdge)
    {
        Rig   rig (L"left 0 band 1", L"left 0 band 0");

        Assert::AreEqual (0L,                       rig.toolBar.GetBounds().left,  L"band 0 against the edge");
        Assert::AreEqual ((long) rig.GetThin(),     rig.stripBar.GetBounds().left, L"band 1 inside it");
    }


    TEST_METHOD (PlacesSavedBeforeBandsStackAsTheyDid)
    {
        Rig   rig (L"top 0", L"top 120");
        int   tall = rig.GetTall();

        AssertRect (RECT { kMargin, 100, 1000 - kMargin, 100 + tall }, rig.stripBar.GetBounds(), L"the timeline against the edge, as before");
        Assert::AreEqual ((long) (100 + tall),        rig.toolBar.GetBounds().top,  L"the command bar inside it");
        Assert::AreEqual ((long) (kMargin + 120),     rig.toolBar.GetBounds().left, L"at its offset");
        Assert::AreEqual ((long) (100 + tall + rig.GetThin()), rig.inner.top);

        Assert::AreEqual (0, rig.strip.GetDock().band, L"each was given a band");
        Assert::AreEqual (1, rig.tool.GetDock().band);
    }


    TEST_METHOD (OldPlacesOnDifferentEdgesEachStandAgainstTheirEdge)
    {
        Rig   rig (L"bottom 0", L"right 40");

        Assert::AreEqual (800L,  rig.stripBar.GetBounds().bottom);
        Assert::AreEqual (1000L, rig.toolBar.GetBounds().right);
        Assert::AreEqual ((long) (100 + 40), rig.toolBar.GetBounds().top);
    }


    TEST_METHOD (ThePlacesRoundTripThroughTheSavedText)
    {
        Rig   first (L"top 500 band 0", L"top 0 band 0");

        first.group.SaveAll();

        Assert::IsTrue (first.toolSaved.ends_with (L" band 0"), first.toolSaved.c_str());

        AssertReloadsAsSaved (first);
    }


    TEST_METHOD (CarryingTheToolbarOverTheStripsBandJoinsIt)
    {
        Rig   rig (L"top 0 band 0", L"top 0 band 1");
        int   tall = rig.GetTall();
        RECT  slot = {};

        rig.Carry (rig.tool, rig.toolBar, { POINT { 400, 100 + tall / 2 } });

        Assert::AreEqual (0, rig.tool.GetDock().band, L"the toolbar is in the strip's band");
        Assert::AreEqual (0, rig.strip.GetDock().band);
        Assert::AreEqual ((long) (100 + tall), rig.inner.top, L"one band left");

        slot = rig.toolBar.GetDropSlot();
        AssertRect (RECT { kMargin, 100, 1000 - kMargin, 100 + tall }, slot, L"the band it is joining is highlighted");

        rig.Release (rig.tool);

        AssertRect (RECT {}, rig.toolBar.GetDropSlot(), L"and the highlight goes once it is put down");
        Assert::IsTrue (rig.toolSaved.ends_with (L" band 0"),  rig.toolSaved.c_str());
        Assert::IsTrue (rig.stripSaved.ends_with (L" band 0"), rig.stripSaved.c_str());
    }


    TEST_METHOD (CarryingTheToolbarAboveTheSharedBandGivesItABandOfItsOwn)
    {
        Rig   rig (L"top 500 band 0", L"top 0 band 0");
        int   thin = rig.GetThin();

        rig.Carry (rig.tool, rig.toolBar, { POINT { 300, 96 } });

        Assert::AreEqual (0, rig.tool.GetDock().band,  L"a new band against the edge");
        Assert::AreEqual (1, rig.strip.GetDock().band, L"the strip's band moved in");
        AssertRect (RECT { kMargin, 100 + thin, 1000 - kMargin, 100 + thin + rig.GetTall() }, rig.stripBar.GetBounds(), L"the strip has the whole row");
        AssertRect (RECT { kMargin, 100, 1000 - kMargin, 100 + thin }, rig.toolBar.GetDropSlot(), L"the new row is highlighted whole");

        rig.Release (rig.tool);

        Assert::IsTrue (rig.stripSaved.ends_with (L" band 1"), L"the neighbor's new band is saved too");
    }


    TEST_METHOD (CarryingTheStripBelowTheSharedBandGivesItABandOfItsOwn)
    {
        Rig   rig (L"top 500 band 0", L"top 0 band 0");
        int   tall = rig.GetTall();

        rig.Carry (rig.strip, rig.stripBar, { POINT { 600, 100 + tall - 2 } });

        Assert::AreEqual (1, rig.strip.GetDock().band, L"a new band inside the toolbar's");
        Assert::AreEqual (0, rig.tool.GetDock().band);
        AssertRect (RECT { kMargin, 100 + rig.GetThin(), 1000 - kMargin, 100 + rig.GetThin() + tall }, rig.stripBar.GetBounds(), L"the whole row");

        rig.Release (rig.strip);
    }


    TEST_METHOD (CarryingTheToolbarAlongTheSharedBandPutsItAfterTheStrip)
    {
        Rig   rig (L"top 500 band 0", L"top 0 band 0");
        int   tall = rig.GetTall();

        rig.Carry (rig.tool, rig.toolBar, { POINT { 300, 100 + tall / 2 }, POINT { 700, 100 + tall / 2 } });

        Assert::AreEqual (0, rig.tool.GetDock().band, L"still in the band");
        Assert::AreEqual (1000L - kMargin, rig.toolBar.GetBounds().right, L"past the strip's middle it goes after the strip");
        Assert::AreEqual ((long) kMargin,  rig.stripBar.GetBounds().left);

        rig.Release (rig.tool);

        AssertReloadsAsSaved (rig);
    }


    //  How far below the top of the area the bands leave the site's pane
    //  starts, with the toolbar sharing its edge, and that top.
    static long MeasurePaneInset (const wchar_t * stripText, const wchar_t * toolText, DxuiDockSite & site, MockDxuiControl & code, long & outInnerTop)
    {
        Rig            rig (stripText, toolText, &site);
        DxuiDpiScaler  scaler;

        site.Layout (rig.inner, scaler);

        outInnerTop = rig.inner.top;
        return code.GetBounds().top - rig.inner.top;
    }


    TEST_METHOD (TheDockSiteSharesTheToolbarsBandOnlyWhenNothingFillsIt)
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        constexpr long   thin     = DxuiToolbar::GetBandDip();
        constexpr long   tall     = thin + kLabelRoom;
        long             innerTop = 0;
        long             apart    = 0;
        long             filled   = 0;
        long             shared   = 0;

        site.AddPane       (L"code", L"Disassembly", &code);
        site.SetPaneLayout (DxuiPaneLayout::MakeSingle (L"code"));

        apart = MeasurePaneInset (L"top 0 band 1", L"top 0 band 0", site, code, innerTop);
        Assert::AreEqual (100 + tall + thin, innerTop, L"a band with another inside it comes out of the area");

        filled = MeasurePaneInset (L"top 500 band 0", L"top 0 band 0", site, code, innerTop);
        Assert::AreEqual (100 + tall, innerTop, L"so does a band the strip fills");
        Assert::AreEqual (apart, filled,        L"and the site has its edge to itself");

        shared = MeasurePaneInset (L"top 0 band 0", L"top 0 band 1", site, code, innerTop);
        Assert::AreEqual (100 + tall,   innerTop, L"the toolbar's own innermost band stays in the area");
        Assert::AreEqual (apart + thin, shared,   L"shared with the site, whose panes start below it");
    }

    //  The site's only group, laid out with the toolbar alone in the innermost
    //  band of its edge, and the toolbar's own rect.
    static RECT MeasureSharedGroup (const wchar_t * toolText, RECT & outBar)
    {
        DxuiDockSite     site;
        MockDxuiControl  code;
        DxuiDpiScaler    scaler;
        Rig              rig (L"top 0 band 0", toolText, &site);

        site.AddPane       (L"code", L"Disassembly", &code);
        site.SetPaneLayout (DxuiPaneLayout::MakeSingle (L"code"));
        site.Layout        (rig.inner, scaler);

        outBar = rig.toolBar.GetBounds();
        return site.GetGroup (0)->GetBounds();
    }


    //  The site lies under the shared band on every edge and takes all of
    //  it, so its panes start where the toolbar ends, never under it.
    TEST_METHOD (TheDockSiteTakesTheWholeSharedBandOnEveryEdge)
    {
        RECT  topBar    = {};
        RECT  bottomBar = {};
        RECT  leftBar   = {};
        RECT  rightBar  = {};
        RECT  top       = MeasureSharedGroup (L"top 0 band 1",    topBar);
        RECT  bottom    = MeasureSharedGroup (L"bottom 0 band 0", bottomBar);
        RECT  left      = MeasureSharedGroup (L"left 0 band 0",   leftBar);
        RECT  right     = MeasureSharedGroup (L"right 0 band 0",  rightBar);



        Assert::AreEqual (topBar.bottom, top.top,       L"below a toolbar across the top");
        Assert::AreEqual (bottomBar.top, bottom.bottom, L"above one across the bottom");
        Assert::AreEqual (leftBar.right, left.left,     L"right of one down the left");
        Assert::AreEqual (rightBar.left, right.right,   L"left of one down the right");
    }

    //  Whether a paint of `bar` tints `slot` in the accent and outlines it.
    static bool IsSlotPainted (DxuiToolbar & bar, const RECT & slot)
    {
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        bool                  tinted   = false;
        bool                  outlined = false;

        bar.Paint (painter, text, theme);

        for (const RecordedPaintCall & call : painter.Calls())
        {
            bool  isSlot = call.x == (float) slot.left && call.y == (float) slot.top &&
                           call.width == (float) (slot.right - slot.left) && call.height == (float) (slot.bottom - slot.top);

            tinted   = tinted   || (isSlot && call.kind == RecordedPaintKind::FillRect    && (call.argb & 0x00FFFFFFu) == (theme.Accent() & 0x00FFFFFFu) && (call.argb >> 24) < 0xFFu);
            outlined = outlined || (isSlot && call.kind == RecordedPaintKind::OutlineRect && call.argb == theme.Accent());
        }

        return tinted && outlined;
    }


    TEST_METHOD (TheCarriedToolbarsBandIsPaintedUnderIt)
    {
        Rig   rig (L"top 500 band 0", L"top 0 band 0");
        RECT  band = { kMargin, 100, 1000 - kMargin, 100 + rig.GetTall() };

        rig.Carry (rig.tool, rig.toolBar, {});

        Assert::IsTrue  (IsSlotPainted (rig.toolBar, band), L"the band, tinted and outlined, while the toolbar is carried");

        rig.Release (rig.tool);

        Assert::IsFalse (IsSlotPainted (rig.toolBar, band), L"and not once it is put down");
    }
};
