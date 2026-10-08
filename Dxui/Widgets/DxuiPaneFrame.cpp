#include "Pch.h"

#include "Widgets/DxuiPaneFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::Geometry
//
//  A spec resolved into the edges every part is placed by. `lineTop` is the
//  first row of the line between the tab band and the pane. The selected
//  tab runs from `tabNear`, the edge it shares with the pane, to `tabFar`,
//  its far edge: up to the pane's top for a document, down to the pane's
//  bottom for a tool window.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiPaneFrame::Geometry
{
    long  left        = 0;
    long  top         = 0;
    long  right       = 0;
    long  bottom      = 0;
    long  t           = 1;
    long  ro          = 0;
    bool  toolWindow  = false;
    long  titlePx     = 0;
    long  bandPx      = 0;
    long  lineTop     = 0;
    long  tabNear     = 0;
    long  tabFar      = 0;
    bool  hasSelected = false;
    long  selLeft     = 0;
    long  selRight    = 0;
    bool  openLeft    = false;
    bool  openRight   = false;
    bool  flushLeft   = false;
    bool  flushRight  = false;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::Build
//
//  Every part of a pane's frame, in the order each phase draws them.
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiPaneFramePart> DxuiPaneFrame::Build (const DxuiPaneFrameSpec & spec)
{
    Geometry                        g = MakeGeometry (spec);
    std::vector<DxuiPaneFramePart>  parts;



    if (g.toolWindow)
    {
        BuildToolWindow (parts, g);
    }
    else
    {
        BuildDocument (parts, g);
    }

    return parts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::GetBodyRect
//
//  Where the pane's content goes: inside the outline, below a document's
//  line or a tool window's title, and above a tool window's line. Never
//  inverted, however small the pane.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiPaneFrame::GetBodyRect (const DxuiPaneFrameSpec & spec)
{
    Geometry  g    = MakeGeometry (spec);
    RECT      body = { g.left + g.t, g.lineTop + g.t, g.right - g.t, g.bottom - g.t };



    if (g.toolWindow)
    {
        body.top    = g.top + g.titlePx;
        body.bottom = (g.bandPx > 0) ? g.lineTop : g.bottom - g.t;
    }

    body.right  = (std::max) (body.right,  body.left);
    body.bottom = (std::max) (body.bottom, body.top);

    return body;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::Paint
//
//  Replays the parts of one phase in order, each inside its clip when it
//  has one and in its role's color.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::Paint (
    IDxuiPainter                          & painter,
    const std::vector<DxuiPaneFramePart>  & parts,
    DxuiPaneFramePhase                      phase,
    const DxuiPaneFrameColors             & colors)
{
    for (const DxuiPaneFramePart & part : parts)
    {
        uint32_t  argb = colors.outline;

        if (part.phase != phase)
        {
            continue;
        }

        switch (part.role)
        {
        case DxuiPaneFrameRole::Gap:     argb = colors.gap;     break;
        case DxuiPaneFrameRole::Band:    argb = colors.band;    break;
        case DxuiPaneFrameRole::Content: argb = colors.content; break;
        case DxuiPaneFrameRole::Outline: argb = colors.outline; break;
        }

        if (part.clipped)
        {
            painter.PushClip ((float) part.clip.left, (float) part.clip.top,
                              (float) (part.clip.right - part.clip.left), (float) (part.clip.bottom - part.clip.top));
        }

        switch (part.shape)
        {
        case DxuiPaneFrameShape::Rect:
            painter.FillRect (part.x, part.y, part.width, part.height, argb);
            break;

        case DxuiPaneFrameShape::RoundedFill:
            painter.FillRoundedRect (part.x, part.y, part.width, part.height, part.radius, argb);
            break;

        case DxuiPaneFrameShape::Ring:
            painter.OutlineRoundedRect (part.x, part.y, part.width, part.height, part.radius, part.thickness, argb);
            break;
        }

        if (part.clipped)
        {
            painter.PopClip();
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::MakeGeometry
//
//  A pane too small for four outer radii across, either way, is drawn with
//  square corners: every corner piece then has an empty box, and the
//  straight runs reach the corners themselves.
//
//  A selected tab is flush with a side of the pane when it starts or ends
//  exactly there and no scroll arrow cuts it off, so its rounded corner is
//  the pane's own.
//
////////////////////////////////////////////////////////////////////////////////

DxuiPaneFrame::Geometry DxuiPaneFrame::MakeGeometry (const DxuiPaneFrameSpec & spec)
{
    constexpr long  kCornersAcross = 4;
    Geometry        g;



    g.left        = spec.pane.left;
    g.top         = spec.pane.top;
    g.right       = spec.pane.right;
    g.bottom      = spec.pane.bottom;
    g.t           = (std::max) (1L, spec.linePx);
    g.ro          = (std::max) (0L, spec.cornerPx);
    g.toolWindow  = spec.toolWindow;
    g.titlePx     = spec.titlePx;
    g.bandPx      = spec.bandPx;
    g.hasSelected = spec.hasSelected && spec.bandPx > 0;
    g.selLeft     = spec.selLeft;
    g.selRight    = spec.selRight;
    g.openLeft    = spec.openLeft;
    g.openRight   = spec.openRight;
    g.flushLeft   = g.hasSelected && !g.openLeft  && g.selLeft  == g.left;
    g.flushRight  = g.hasSelected && !g.openRight && g.selRight == g.right;

    if (g.right - g.left < kCornersAcross * g.ro || g.bottom - g.top < kCornersAcross * g.ro)
    {
        g.ro = 0;
    }

    if (g.toolWindow)
    {
        g.lineTop = (g.bandPx > 0) ? g.bottom - g.bandPx - g.t : g.bottom;
        g.tabNear = g.lineTop;
        g.tabFar  = g.bottom;
    }
    else
    {
        g.lineTop = g.top + g.bandPx;
        g.tabNear = g.lineTop + g.t;
        g.tabFar  = g.top;
    }

    return g;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::BuildDocument
//
//  Tabs on top. Under the group's own paint: the gap outside the band's
//  rounded top corners, the band, and the joins' fillets. Over everything:
//  the gap outside the pane's rounded bottom corners, then the outline --
//  the line along the band, square where it meets the pane's sides, the
//  sides and bottom with their rounded corners, and round the selected tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::BuildDocument (std::vector<DxuiPaneFramePart> & parts, const Geometry & g)
{
    long  pl = g.left;
    long  pt = g.top;
    long  pr = g.right;
    long  pb = g.bottom;
    long  t  = g.t;
    long  ro = g.ro;
    long  yl = g.lineTop;



    AddRect        (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { pl,      pt, pl + ro, pt + ro });
    AddRect        (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { pr - ro, pt, pr,      pt + ro });
    AddRoundedFill (parts, DxuiPaneFrameRole::Band, RECT { pl, pt, pr, yl }, RECT { pl, pt, pr, yl + ro }, ro);
    AddJoins       (parts, g, DxuiPaneFramePhase::Under);

    AddCap         (parts, g, RECT { pl,      pb - ro, pl + ro, pb });
    AddCap         (parts, g, RECT { pr - ro, pb - ro, pr,      pb });
    AddLineRuns    (parts, g);
    AddRect        (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { pl,      g.flushLeft  ? pt + ro : yl, pl + t,  pb - ro });
    AddRect        (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { pr - t,  g.flushRight ? pt + ro : yl, pr,      pb - ro });
    AddRect        (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { pl + ro, pb - t,                       pr - ro, pb      });
    AddQuarterRing (parts, g, pl + ro, pb - ro, RECT { pl,      pb - ro, pl + ro, pb });
    AddQuarterRing (parts, g, pr - ro, pb - ro, RECT { pr - ro, pb - ro, pr,      pb });
    AddTabOutline  (parts, g);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::BuildToolWindow
//
//  A title along the top in the content color, rounded at the pane's top
//  corners. Without tabs the outline runs right round the pane, with the
//  gap outside its rounded bottom corners. With tabs, they hang in a band
//  below the line under the pane, rounded at its bottom corners; the
//  outline runs down the pane's sides to that line, square where it meets
//  it, and on round the selected tab.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::BuildToolWindow (std::vector<DxuiPaneFramePart> & parts, const Geometry & g)
{
    long  pl       = g.left;
    long  pt       = g.top;
    long  pr       = g.right;
    long  pb       = g.bottom;
    long  t        = g.t;
    long  ro       = g.ro;
    long  bandTop  = pb - g.bandPx;
    bool  hasStrip = g.bandPx > 0;
    long  leftEnd  = (!hasStrip || g.flushLeft)  ? pb - ro : g.lineTop + t;
    long  rightEnd = (!hasStrip || g.flushRight) ? pb - ro : g.lineTop + t;



    AddRect        (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { pl,      pt, pl + ro, pt + ro });
    AddRect        (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { pr - ro, pt, pr,      pt + ro });
    AddRoundedFill (parts, DxuiPaneFrameRole::Content, RECT { pl, pt, pr, pt + g.titlePx }, RECT { pl, pt, pr, pt + g.titlePx + ro }, ro);

    if (hasStrip)
    {
        AddRect        (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { pl,      pb - ro, pl + ro, pb });
        AddRect        (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Gap, RECT { pr - ro, pb - ro, pr,      pb });
        AddRoundedFill (parts, DxuiPaneFrameRole::Band, RECT { pl, bandTop, pr, pb }, RECT { pl, bandTop - ro, pr, pb }, ro);
        AddJoins       (parts, g, DxuiPaneFramePhase::Under);
    }
    else
    {
        AddCap (parts, g, RECT { pl,      pb - ro, pl + ro, pb });
        AddCap (parts, g, RECT { pr - ro, pb - ro, pr,      pb });
    }

    AddRect        (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { pl + ro, pt,      pr - ro, pt + t   });
    AddQuarterRing (parts, g, pl + ro, pt + ro, RECT { pl,      pt, pl + ro, pt + ro });
    AddQuarterRing (parts, g, pr - ro, pt + ro, RECT { pr - ro, pt, pr,      pt + ro });
    AddRect        (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { pl,      pt + ro, pl + t,  leftEnd  });
    AddRect        (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { pr - t,  pt + ro, pr,      rightEnd });

    if (hasStrip)
    {
        AddLineRuns   (parts, g);
        AddTabOutline (parts, g);
    }
    else
    {
        AddRect        (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { pl + ro, pb - t, pr - ro, pb });
        AddQuarterRing (parts, g, pl + ro, pb - ro, RECT { pl,      pb - ro, pl + ro, pb });
        AddQuarterRing (parts, g, pr - ro, pb - ro, RECT { pr - ro, pb - ro, pr,      pb });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::AddLineRuns
//
//  The line between the tab band and the pane, from each side of the pane
//  to the selected tab's join. It meets a side square, and stops at the
//  side's own end where the tab is flush with that side, or at the scroll
//  arrow that cuts the tab off. With no tab selected it runs the pane's
//  full width.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::AddLineRuns (std::vector<DxuiPaneFramePart> & parts, const Geometry & g)
{
    long  joinLeft  = g.flushLeft  ? g.left  : g.openLeft  ? g.selLeft  : g.selLeft  + g.t - g.ro;
    long  joinRight = g.flushRight ? g.right : g.openRight ? g.selRight : g.selRight - g.t + g.ro;
    long  lineEnd   = g.hasSelected ? joinLeft  : g.right;
    long  lineStart = g.hasSelected ? joinRight : g.right;



    AddRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { g.left,    g.lineTop, lineEnd, g.lineTop + g.t });
    AddRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, RECT { lineStart, g.lineTop, g.right, g.lineTop + g.t });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::AddJoins
//
//  Where the selected tab's side meets the line, on each side that is
//  neither flush with the pane nor cut off: in the Under phase, a fillet of
//  the content color outside a circle of the outer radius, which flares the
//  tab into the line; in the Over phase, the quarter of the outline round
//  that circle. The circle's center is one radius out from the tab's side
//  and one radius from the line, so its ring is tangent to the tab side's
//  own column and to the line's own row.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::AddJoins (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, DxuiPaneFramePhase phase)
{
    long  centerY = (g.tabFar < g.tabNear) ? g.tabNear - g.ro : g.tabNear + g.ro;
    long  leftX   = g.selLeft  + g.t - g.ro;
    long  rightX  = g.selRight - g.t + g.ro;
    RECT  boxL    = GetTabRows (g, leftX,             g.selLeft + g.t, 0, g.ro);
    RECT  boxR    = GetTabRows (g, g.selRight - g.t,  rightX,          0, g.ro);
    bool  joinL   = g.hasSelected && !g.flushLeft  && !g.openLeft;
    bool  joinR   = g.hasSelected && !g.flushRight && !g.openRight;



    if (joinL && phase == DxuiPaneFramePhase::Under)
    {
        AddFillet (parts, g, leftX, centerY, boxL);
    }
    else if (joinL)
    {
        AddQuarterRing (parts, g, leftX, centerY, boxL);
    }

    if (joinR && phase == DxuiPaneFramePhase::Under)
    {
        AddFillet (parts, g, rightX, centerY, boxR);
    }
    else if (joinR)
    {
        AddQuarterRing (parts, g, rightX, centerY, boxR);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::AddTabOutline
//
//  The outline round the selected tab: its far edge, its sides down to the
//  joins, its rounded far corners, and the joins. A side flush with the pane
//  is the pane's own side, and a side cut off by a scroll arrow has no side,
//  corner or join; the far edge runs on to the arrow.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::AddTabOutline (std::vector<DxuiPaneFramePart> & parts, const Geometry & g)
{
    long  depth = std::abs (g.tabFar - g.tabNear);
    long  ringY = (g.tabFar < g.tabNear) ? g.tabFar + g.ro : g.tabFar - g.ro;
    long  sl    = g.selLeft;
    long  sr    = g.selRight;
    long  t     = g.t;
    long  ro    = g.ro;
    long  edgeL = g.openLeft  ? sl : sl + ro;
    long  edgeR = g.openRight ? sr : sr - ro;
    bool  sideL = !g.flushLeft  && !g.openLeft;
    bool  sideR = !g.flushRight && !g.openRight;



    if (!g.hasSelected)
    {
        return;
    }

    AddRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, GetTabRows (g, edgeL, edgeR, depth - t, t));

    if (sideL)
    {
        AddRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, GetTabRows (g, sl, sl + t, ro, depth - 2 * ro));
    }

    if (sideR)
    {
        AddRect (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, GetTabRows (g, sr - t, sr, ro, depth - 2 * ro));
    }

    if (!g.openLeft)
    {
        AddQuarterRing (parts, g, sl + ro, ringY, GetTabRows (g, sl, sl + ro, depth - ro, ro));
    }

    if (!g.openRight)
    {
        AddQuarterRing (parts, g, sr - ro, ringY, GetTabRows (g, sr - ro, sr, depth - ro, ro));
    }

    AddJoins (parts, g, DxuiPaneFramePhase::Over);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::GetTabRows
//
//  A rect across the selected tab from `left` to `right`, `extent` rows
//  deep, starting `distance` rows from the edge the tab shares with the pane
//  and running toward its far edge. A negative extent is an empty rect.
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiPaneFrame::GetTabRows (const Geometry & g, long left, long right, long distance, long extent)
{
    if (g.tabFar < g.tabNear)
    {
        return RECT { left, g.tabNear - distance - extent, right, g.tabNear - distance };
    }

    return RECT { left, g.tabNear + distance, right, g.tabNear + distance + extent };
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::AddRect
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::AddRect (std::vector<DxuiPaneFramePart> & parts, DxuiPaneFramePhase phase, DxuiPaneFrameRole role, const RECT & rect)
{
    DxuiPaneFramePart  part;



    if (IsEmpty (rect))
    {
        return;
    }

    part.shape  = DxuiPaneFrameShape::Rect;
    part.phase  = phase;
    part.role   = role;
    part.x      = (float) rect.left;
    part.y      = (float) rect.top;
    part.width  = (float) (rect.right - rect.left);
    part.height = (float) (rect.bottom - rect.top);

    parts.push_back (part);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::AddRoundedFill
//
//  A fill of `rect`, its corners rounded, seen only inside `clip`: a band
//  or a title whose outer corners are rounded and whose edge along the pane
//  is square. With no radius it is a plain fill of what the clip shows.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::AddRoundedFill (std::vector<DxuiPaneFramePart> & parts, DxuiPaneFrameRole role, const RECT & clip, const RECT & rect, long radius)
{
    DxuiPaneFramePart  part;
    RECT               shown = { (std::max) (clip.left,  rect.left),  (std::max) (clip.top,    rect.top),
                                 (std::min) (clip.right, rect.right), (std::min) (clip.bottom, rect.bottom) };



    if (radius <= 0)
    {
        AddRect (parts, DxuiPaneFramePhase::Under, role, shown);
        return;
    }

    if (IsEmpty (clip))
    {
        return;
    }

    part.shape   = DxuiPaneFrameShape::RoundedFill;
    part.phase   = DxuiPaneFramePhase::Under;
    part.role    = role;
    part.clipped = true;
    part.clip    = clip;
    part.x       = (float) rect.left;
    part.y       = (float) rect.top;
    part.width   = (float) (rect.right - rect.left);
    part.height  = (float) (rect.bottom - rect.top);
    part.radius  = (float) radius;

    parts.push_back (part);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::AddRing
//
//  A ring inside the rounded edge of `rect`, `thickness` deep, seen only inside
//  `clip`.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::AddRing (
    std::vector<DxuiPaneFramePart>  & parts,
    DxuiPaneFramePhase                phase,
    DxuiPaneFrameRole                 role,
    const RECT                      & clip,
    const RECT                      & rect,
    long                              radius,
    long                              thickness)
{
    DxuiPaneFramePart  part;



    if (IsEmpty (clip) || radius <= 0)
    {
        return;
    }

    part.shape     = DxuiPaneFrameShape::Ring;
    part.phase     = phase;
    part.role      = role;
    part.clipped   = true;
    part.clip      = clip;
    part.x         = (float) rect.left;
    part.y         = (float) rect.top;
    part.width     = (float) (rect.right - rect.left);
    part.height    = (float) (rect.bottom - rect.top);
    part.radius    = (float) radius;
    part.thickness = (float) thickness;

    parts.push_back (part);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::AddQuarterRing
//
//  The quarter of the outline round a corner whose circle is centered at
//  (cx, cy): the outline's ring at the outer radius, inside the corner's
//  box.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::AddQuarterRing (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, long cx, long cy, const RECT & box)
{
    AddRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Outline, box,
             RECT { cx - g.ro, cy - g.ro, cx + g.ro, cy + g.ro }, g.ro, g.t);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::AddFillet
//
//  The content color over a join's box, outside the circle of the outer
//  radius centered at (cx, cy). A ring's inner edge is its rounded box
//  shrunk by its thickness, so a ring twice the radius and one radius thick
//  covers everything in the box beyond the circle.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::AddFillet (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, long cx, long cy, const RECT & box)
{
    AddRing (parts, DxuiPaneFramePhase::Under, DxuiPaneFrameRole::Content, box,
             RECT { cx - 2 * g.ro, cy - 2 * g.ro, cx + 2 * g.ro, cy + 2 * g.ro }, 2 * g.ro, g.ro);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::AddCap
//
//  The gap color over a corner's box, outside the pane's own rounded
//  corner: a ring one radius thick round the pane grown by a radius, whose
//  inner edge is the pane's outer edge rounded at the outer radius.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiPaneFrame::AddCap (std::vector<DxuiPaneFramePart> & parts, const Geometry & g, const RECT & box)
{
    AddRing (parts, DxuiPaneFramePhase::Over, DxuiPaneFrameRole::Gap, box,
             RECT { g.left - g.ro, g.top - g.ro, g.right + g.ro, g.bottom + g.ro }, 2 * g.ro, g.ro);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneFrame::IsEmpty
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiPaneFrame::IsEmpty (const RECT & rect)
{
    return rect.right <= rect.left || rect.bottom <= rect.top;
}





