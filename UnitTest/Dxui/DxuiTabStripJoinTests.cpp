#include "Pch.h"

#include "MockDxuiPainter.h"
#include "MockDxuiTextRenderer.h"
#include "MockDxuiTheme.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabStripJoinTests
//
//  Where the selected tab joins its pane, seen through the strip itself. The
//  selected tab once drew its flares one line outside the tab, with circles,
//  and painted the band color back over them -- across a hovered neighbor's
//  pill. Now it draws its body inside the tab and the line it opens into,
//  after every other tab, and the pills stand clear of the rows where the
//  frame's joins curve. A selected tab ending just short of the strip's end
//  is drawn reaching it, and one cut off under a scroll arrow is square at
//  the cut, as the frame draws them.
//
////////////////////////////////////////////////////////////////////////////////

namespace DxuiTabStripJoinTests
{
    struct Strip
    {
        DxuiTabStrip          strip;
        DxuiDpiScaler         scaler;
        MockDxuiPainter       painter;
        MockDxuiTextRenderer  text;
        MockDxuiTheme         theme;
        RECT                  bounds = {};
        long                  t      = 1;
    };



    //  Tabs between `edges`, tab `selected` selected, in a strip `widthPx`
    //  wide and one band high at `dpi`: along a pane's top for a document,
    //  along its bottom for a tool window.
    static void LayOutTabs (Strip & s, DxuiTabStrip::Style style, UINT dpi, long widthPx, const std::vector<long> & edges, int selected)
    {
        constexpr long                  kTop = 100;
        long                            band = 0;
        std::vector<DxuiTabStrip::Tab>  tabs;



        s.scaler.SetDpi (dpi);
        band     = s.scaler.ToPx (DxuiTabGroup::kStripDip);
        s.t      = DxuiPaneMetrics::GetLinePx (s.scaler);
        s.bounds = RECT { 0, kTop, widthPx, kTop + band };

        for (size_t i = 0; i + 1 < edges.size(); i++)
        {
            DxuiTabStrip::Tab  tab;

            tab.rect  = RECT { edges[i], s.bounds.top, edges[i + 1], s.bounds.bottom };
            tab.label = L"Tab";
            tabs.push_back (tab);
        }

        s.strip.SetStyle    (style);
        s.strip.SetTabs     (tabs);
        s.strip.Layout      (s.bounds, s.scaler);
        s.strip.SetSelected (selected);
    }



    //  Three tabs, the middle one selected and the first under the pointer,
    //  in a strip 600 px wide.
    static void PaintStrip (Strip & s, DxuiTabStrip::Style style, UINT dpi)
    {
        constexpr long  kWidthPx = 600;
        constexpr long  kEdges[] = { 0, 130, 290, 400 };



        LayOutTabs (s, style, dpi, kWidthPx, std::vector<long> (std::begin (kEdges), std::end (kEdges)), 1);
        s.strip.SetMouseHover ((kEdges[0] + kEdges[1]) / 2, (s.bounds.top + s.bounds.bottom) / 2);
        s.strip.Paint         (s.painter, s.text, s.theme);
    }



    //  The selected tab's body: its rounded fill in the content color.
    static bool TryFindBody (const Strip & s, RecordedPaintCall & body)
    {
        bool  found = false;



        for (const RecordedPaintCall & call : s.painter.Calls())
        {
            if (call.kind == RecordedPaintKind::FillRoundedRect && call.argb == s.theme.ContentBackground())
            {
                body  = call;
                found = true;
            }
        }

        return found;
    }



    //  What a call can reach: its rect, cut to the clip in force.
    static RECT GetReach (const RecordedPaintCall & call)
    {
        RECT  r = { std::lround (call.x), std::lround (call.y), std::lround (call.x + call.width), std::lround (call.y + call.height) };



        if (call.isClipped)
        {
            r = RECT { (std::max) (r.left,  call.clip.left),  (std::max) (r.top,    call.clip.top),
                       (std::min) (r.right, call.clip.right), (std::min) (r.bottom, call.clip.bottom) };
        }

        return r;
    }



    static bool Overlaps (const RECT & a, const RECT & b)
    {
        return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
    }



    static bool Contains (const RECT & outer, const RECT & inner)
    {
        return inner.left >= outer.left && inner.top >= outer.top && inner.right <= outer.right && inner.bottom <= outer.bottom;
    }



    //  The hovered first tab's pill: where it is in the log, and its rect.
    static bool TryFindPill (const Strip & s, size_t & index, RECT & pill)
    {
        uint32_t  hover = (s.theme.Foreground() & 0x00FFFFFFu) | 0x14000000u;
        bool      found = false;



        for (size_t i = 0; i < s.painter.Calls().size(); i++)
        {
            const RecordedPaintCall  & call = s.painter.Calls()[i];

            if (!found && call.kind == RecordedPaintKind::FillRoundedRect && call.argb == hover && std::lround (call.x) == 0)
            {
                index = i;
                pill  = GetReach (call);
                found = true;
            }
        }

        return found;
    }



    static DxuiPaneFrameSpec MakeFrameSpec (const Strip & s, bool toolWindow)
    {
        constexpr long     kPaneDepth = 400;
        DxuiPaneFrameSpec  spec;



        spec.toolWindow  = toolWindow;
        spec.bandPx      = s.bounds.bottom - s.bounds.top;
        spec.titlePx     = toolWindow ? s.scaler.ToPx (DxuiTabGroup::kTitleDip) : 0;
        spec.pane        = toolWindow ? RECT { 0, s.bounds.bottom - kPaneDepth, 600, s.bounds.bottom } : RECT { 0, s.bounds.top, 600, s.bounds.top + kPaneDepth };
        spec.hasSelected = true;
        spec.selLeft     = 130;
        spec.selRight    = 290;
        spec.linePx      = DxuiPaneMetrics::GetLinePx (s.scaler);
        spec.cornerPx    = DxuiPaneMetrics::GetCornerPx (s.scaler);
        return spec;
    }



    TEST_CLASS (DxuiTabStripJoinTests)
    {
    public:

        //  At 150%, in both compact styles: no circles, nothing drawn over the
        //  hovered neighbor's pill once it is down, and the selected tab's
        //  fill held to the tab and the line it opens into.
        TEST_METHOD (TheSelectedTabStaysInItsTabAndOffItsNeighborsPill)
        {
            for (DxuiTabStrip::Style style : { DxuiTabStrip::Style::Document, DxuiTabStrip::Style::ToolWindow })
            {
                Strip         s;
                bool          below   = style == DxuiTabStrip::Style::ToolWindow;
                size_t        index   = 0;
                RECT          pill    = {};
                RECT          allowed = {};
                size_t        fills   = 0;
                std::wstring  kind    = below ? L"tool window" : L"document";

                PaintStrip (s, style, 144);
                allowed = below ? RECT { 130, s.bounds.top - s.t, 290, s.bounds.bottom } : RECT { 130, s.bounds.top, 290, s.bounds.bottom + s.t };

                Assert::IsTrue (TryFindPill (s, index, pill), (kind + L": the hovered tab's pill").c_str());

                for (size_t i = 0; i < s.painter.Calls().size(); i++)
                {
                    const RecordedPaintCall  & call  = s.painter.Calls()[i];
                    RECT                       reach = GetReach (call);

                    Assert::IsFalse (call.kind == RecordedPaintKind::FillCircle, (kind + L": no circle").c_str());

                    if (i > index)
                    {
                        Assert::IsFalse (Overlaps (reach, pill), (kind + L": nothing lands on the pill after it").c_str());
                    }

                    if (call.argb == s.theme.ContentBackground())
                    {
                        fills++;
                        Assert::IsTrue (Contains (allowed, reach), (kind + L": the selected fill stays in the tab and its line").c_str());
                    }
                }

                Assert::IsTrue (fills > 0, (kind + L": the selected tab is filled").c_str());
            }
        }


        //  Over a range of scales, a hovered pill never reaches into the
        //  boxes where the frame's joins curve into the line.
        TEST_METHOD (TheHoverInsetKeepsPillsOutOfTheJoins)
        {
            constexpr int  kLeastInsetDip = 3;



            for (UINT dpi = 96; dpi <= 288; dpi += 24)
            {
                for (DxuiTabStrip::Style style : { DxuiTabStrip::Style::Document, DxuiTabStrip::Style::ToolWindow })
                {
                    Strip                           s;
                    bool                            below   = style == DxuiTabStrip::Style::ToolWindow;
                    size_t                          index   = 0;
                    RECT                            pill    = {};
                    size_t                          joins   = 0;
                    std::vector<DxuiPaneFramePart>  parts;
                    std::wstring                    where   = std::format (L"{} dpi, {}", dpi, below ? L"tool window" : L"document");

                    PaintStrip (s, style, dpi);
                    parts = DxuiPaneFrame::Build (MakeFrameSpec (s, below));

                    Assert::AreEqual ((std::max) (s.scaler.ToPx (kLeastInsetDip), DxuiPaneMetrics::GetCornerPx (s.scaler) - DxuiPaneMetrics::GetLinePx (s.scaler)),
                                      DxuiTabStrip::GetHoverInsetPx (s.scaler), (where + L": 3 DIP, or the outer radius less a line").c_str());
                    Assert::IsTrue (TryFindPill (s, index, pill), (where + L": the hovered tab's pill").c_str());

                    for (const DxuiPaneFramePart & part : parts)
                    {
                        if (part.shape != DxuiPaneFrameShape::Ring || part.role != DxuiPaneFrameRole::Content)
                        {
                            continue;
                        }

                        joins++;
                        Assert::IsFalse (Overlaps (pill, part.clip), (where + L": the pill stays out of a join").c_str());
                    }

                    Assert::AreEqual ((size_t) 2, joins, (where + L": a join each side of the selected tab").c_str());
                }
            }
        }


        //  A hovered tab's pill is as wide as the tab, stands in 4, 5 and 6 px
        //  from the strip's edges at 100%, 125% and 150%, and is rounded at
        //  4 DIP.
        TEST_METHOD (ThePillStandsInByTheHoverInset)
        {
            constexpr UINT  kDpis[]   = { 96, 120, 144 };
            constexpr long  kInsets[] = { 4, 5, 6 };
            constexpr long  kRadii[]  = { 4, 5, 6 };



            for (size_t i = 0; i < std::size (kDpis); i++)
            {
                for (DxuiTabStrip::Style style : { DxuiTabStrip::Style::Document, DxuiTabStrip::Style::ToolWindow })
                {
                    Strip              s;
                    size_t             index = 0;
                    RECT               pill  = {};
                    RecordedPaintCall  call;
                    std::wstring       where = std::format (L"{} dpi, {}", kDpis[i], (style == DxuiTabStrip::Style::ToolWindow) ? L"tool window" : L"document");

                    PaintStrip (s, style, kDpis[i]);

                    Assert::AreEqual ((int) kInsets[i], DxuiTabStrip::GetHoverInsetPx (s.scaler), where.c_str());
                    Assert::IsTrue   (TryFindPill (s, index, pill), (where + L": the hovered tab's pill").c_str());

                    call = s.painter.Calls()[index];

                    Assert::AreEqual (130.0f,                                                          call.width,  (where + L": the tab's width").c_str());
                    Assert::AreEqual ((float) (s.bounds.top + kInsets[i]),                             call.y,      (where + L": in from the strip's top").c_str());
                    Assert::AreEqual ((float) (s.bounds.bottom - s.bounds.top - 2 * kInsets[i]),       call.height, (where + L": and its bottom").c_str());
                    Assert::AreEqual ((float) kRadii[i],                                               call.radius, (where + L": the radius").c_str());
                }
            }
        }


        //  A selected tab that ends just short of the strip's end, nearer than
        //  the pane's frame would leave room for a join, is drawn reaching it,
        //  as the frame draws it flush with the pane's side. One that ends
        //  that far short keeps its own end.
        TEST_METHOD (ATabEndingJustShortOfTheStripsEndReachesIt)
        {
            constexpr long  kWidthPx = 600;
            constexpr long  kShortPx = 2;



            for (UINT dpi : { 96u, 120u, 144u })
            {
                for (DxuiTabStrip::Style style : { DxuiTabStrip::Style::Document, DxuiTabStrip::Style::ToolWindow })
                {
                    Strip              s;
                    Strip              shorter;
                    long               reach     = 0;
                    long               left      = 0;
                    long               right     = 0;
                    bool               openLeft  = false;
                    bool               openRight = false;
                    RecordedPaintCall  body;
                    std::wstring       where     = std::format (L"{} dpi, {}", dpi, (style == DxuiTabStrip::Style::ToolWindow) ? L"tool window" : L"document");

                    LayOutTabs (s, style, dpi, kWidthPx, { 0, 130, 290, kWidthPx - kShortPx }, 2);
                    s.strip.Paint (s.painter, s.text, s.theme);
                    reach = DxuiPaneFrame::GetFlushReachPx (DxuiPaneMetrics::GetCornerPx (s.scaler), DxuiPaneMetrics::GetLinePx (s.scaler));

                    Assert::IsTrue   (kShortPx < reach, (where + L": the tab ends nearer than a join's reach").c_str());
                    Assert::IsTrue   (s.strip.GetSelectedSpan (left, right, openLeft, openRight), where.c_str());
                    Assert::IsFalse  (openRight, where.c_str());
                    Assert::AreEqual (kWidthPx, right, (where + L": the span reaches the strip's end").c_str());
                    Assert::IsTrue   (TryFindBody (s, body), (where + L": the selected tab's body").c_str());
                    Assert::AreEqual ((float) kWidthPx, body.x + body.width, (where + L": the body reaches the strip's end").c_str());
                    Assert::AreEqual (kWidthPx, body.clip.right, (where + L": and shows to it").c_str());

                    LayOutTabs (shorter, style, dpi, kWidthPx, { 0, 130, 290, kWidthPx - reach }, 2);
                    Assert::IsTrue   (shorter.strip.GetSelectedSpan (left, right, openLeft, openRight), where.c_str());
                    Assert::AreEqual (kWidthPx - reach, right, (where + L": a tab a join's reach short keeps its end").c_str());
                }
            }
        }


        //  A selected tab cut off under the left scroll arrow by less than the
        //  outer radius is square at the cut, where the frame runs its top
        //  straight on to the arrow: its fill's rounded corner lies past the
        //  cut, out of sight.
        TEST_METHOD (ATabCutOffUnderAnArrowIsSquareAtTheCut)
        {
            constexpr UINT   kDpi          = 120;
            constexpr long   kWidthPx      = 300;
            constexpr long   kTabLeft      = 120;
            constexpr int    kArrowDip     = 28;     // the strip's scroll arrow
            constexpr int    kWheelStepDip = 60;     // the strip's scroll per wheel notch
            constexpr float  kRoundDown    = 0.5f;   // past the step's whole pixels, so the scroll truncates to the target
            DxuiDpiScaler    scaler;
            long             ro            = 0;



            scaler.SetDpi (kDpi);
            ro = DxuiPaneMetrics::GetCornerPx (scaler);

            for (long cut = 1; cut < ro; cut++)
            {
                Strip              s;
                long               arrow  = 0;
                long               target = 0;
                float              step   = 0.0f;
                RecordedPaintCall  body;
                std::wstring       where  = std::format (L"cut {} px", cut);

                LayOutTabs (s, DxuiTabStrip::Style::Document, kDpi, kWidthPx, { 0, kTabLeft, 240, 360, 480 }, 1);

                arrow  = s.scaler.ToPx (kArrowDip);
                target = kTabLeft + cut;
                step   = (float) s.scaler.ToPx (kWheelStepDip);

                Assert::IsTrue   (s.strip.HasScrollArrows(), where.c_str());
                s.strip.OnWheel  (-((float) (target - s.strip.GetScrollPx()) + kRoundDown) / step);
                Assert::AreEqual ((int) target, s.strip.GetScrollPx(), (where + L": scrolled to cut the tab").c_str());
                Assert::AreEqual (arrow - cut, s.strip.GetTabScreenRect (1).left, where.c_str());

                s.strip.Paint (s.painter, s.text, s.theme);

                Assert::IsTrue   (TryFindBody (s, body), (where + L": the selected tab's body").c_str());
                Assert::IsTrue   (body.isClipped, where.c_str());
                Assert::AreEqual (arrow, body.clip.left, (where + L": shown from the arrow").c_str());
                Assert::IsTrue   (body.x + body.radius <= (float) body.clip.left, (where + L": its rounded corner lies past the cut").c_str());
            }
        }
    };
}
