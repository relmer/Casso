#include "Pch.h"

#include "PrinterViewport.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::PrinterViewport
//
////////////////////////////////////////////////////////////////////////////////

PrinterViewport::PrinterViewport (const Config & cfg)
    : m_cfg (cfg)
{
    if (m_cfg.viewportRows < 1)
    {
        m_cfg.viewportRows = 1;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::Advance
//
////////////////////////////////////////////////////////////////////////////////

void PrinterViewport::Advance (int liveRow)
{
    if (liveRow > m_liveRow)
    {
        m_liveRow = liveRow;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::Scroll
//
//  Moves the view's bottom anchor by deltaRows and clamps it to the strip:
//  no further back than a full viewport against the top of the paper, no
//  further forward than the live row. Reaching the live row hands control
//  back to Following, so a user who scrolls all the way down "catches" the
//  print head again.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterViewport::Scroll (int deltaRows, int64_t nowMs)
{
    int   bottom = std::clamp (BottomRow () + deltaRows, MinBottomRow (), m_liveRow);

    // The clamp decides the mode: landing on the live row (including any
    // scroll on a strip still shorter than one viewport, where there is
    // nowhere to go) means Following; anywhere earlier means Scrolled.
    m_following = (bottom >= m_liveRow);

    if (!m_following)
    {
        m_anchorBottom = bottom;
    }

    m_lastScrollMs = nowMs;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::Tick
//
////////////////////////////////////////////////////////////////////////////////

void PrinterViewport::Tick (int64_t nowMs)
{
    if (!m_following && nowMs - m_lastScrollMs >= m_cfg.snapDelayMs)
    {
        m_following = true;   // idle long enough: snap back to the live row
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::VisibleSpan
//
////////////////////////////////////////////////////////////////////////////////

PrinterViewport::Span PrinterViewport::VisibleSpan () const
{
    Span   span;

    span.lastRow  = BottomRow ();
    span.firstRow = (std::max) (0, span.lastRow - m_cfg.viewportRows + 1);

    return span;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::Reset
//
////////////////////////////////////////////////////////////////////////////////

void PrinterViewport::Reset ()
{
    m_liveRow      = 0;
    m_anchorBottom = 0;
    m_lastScrollMs = 0;
    m_following    = true;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::BottomRow
//
//  The view's current bottom row. Following rides the live row; Scrolled uses
//  the absolute anchor, re-clamped in case the config or strip shrank the
//  legal range after the anchor was recorded.
//
////////////////////////////////////////////////////////////////////////////////

int PrinterViewport::BottomRow () const
{
    if (m_following)
    {
        return m_liveRow;
    }

    return std::clamp (m_anchorBottom, MinBottomRow (), m_liveRow);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::MinBottomRow
//
//  The furthest back the bottom may scroll: a full viewport against the top
//  of the paper -- unless the strip is still shorter than one viewport, in
//  which case there is nowhere to scroll and the bottom stays on the live row.
//
////////////////////////////////////////////////////////////////////////////////

int PrinterViewport::MinBottomRow () const
{
    return (std::min) (m_liveRow, m_cfg.viewportRows - 1);
}
