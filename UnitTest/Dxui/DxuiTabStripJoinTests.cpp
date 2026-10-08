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
//  frame's joins curve.
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



    //  Three tabs, the middle one selected and the first under the pointer,
    //  in a strip one band high at `dpi`: along a pane's top for a document,
    //  along its bottom for a tool window.
    static void PaintStrip (Strip & s, DxuiTabStrip::Style style, UINT dpi)
    {
        constexpr long                  kTop      = 100;
        constexpr long                  kEdges[]  = { 0, 130, 290, 400 };
        long                            band      = 0;
        std::vector<DxuiTabStrip::Tab>  tabs;



        s.scaler.SetDpi (dpi);
        band     = s.scaler.ToPx (DxuiTabGroup::kStripDip);
        s.t      = DxuiPaneMetrics::GetLinePx (s.scaler);
        s.bounds = RECT { 0, kTop, 600, kTop + band };

        for (size_t i = 0; i + 1 < std::size (kEdges); i++)
        {
            DxuiTabStrip::Tab  tab;

            tab.rect  = RECT { kEdges[i], s.bounds.top, kEdges[i + 1], s.bounds.bottom };
            tab.label = L"Tab";
            tabs.push_back (tab);
        }

        s.strip.SetStyle      (style);
        s.strip.SetTabs       (tabs);
        s.strip.Layout        (s.bounds, s.scaler);
        s.strip.SetSelected   (1);
        s.strip.SetMouseHover ((kEdges[0] + kEdges[1]) / 2, (s.bounds.top + s.bounds.bottom) / 2);
        s.strip.Paint         (s.painter, s.text, s.theme);
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

                    Assert::IsTrue (DxuiTabStrip::GetHoverInsetPx (s.scaler) >= DxuiPaneMetrics::GetCornerPx (s.scaler) - DxuiPaneMetrics::GetLinePx (s.scaler),
                                    where.c_str());
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
    };
}
