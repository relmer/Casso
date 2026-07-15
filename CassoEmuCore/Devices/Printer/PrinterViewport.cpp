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
//  PrinterViewport::NotifyUserScroll
//
//  The user grabbed the scroll (panZoom moved panY on their input): leave
//  Following so the view stays where they park it, and record the moment plus
//  the live row so the idle snap only fires once printing advances past here.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterViewport::NotifyUserScroll (int64_t nowMs)
{
    m_following       = false;
    m_liveRowAtScroll = m_liveRow;
    m_lastScrollMs    = nowMs;
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::Tick
//
////////////////////////////////////////////////////////////////////////////////

void PrinterViewport::Tick (int64_t nowMs)
{
    // Snap back only when there is a live print to return to: the live row
    // must have advanced since the user scrolled. A finished print stays
    // where the user put it (scrolled back OR overscrolled forward).
    if (!m_following &&
        m_liveRow > m_liveRowAtScroll &&
        nowMs - m_lastScrollMs >= m_cfg.snapDelayMs)
    {
        m_following = true;
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::MinBottomRow
//
//  The furthest back the bottom may scroll: a full viewport against the top
//  of the paper -- unless the strip is still shorter than one viewport, in
//  which case there is nowhere to scroll and the bottom pins to the live row.
//
////////////////////////////////////////////////////////////////////////////////

int PrinterViewport::MinBottomRow () const
{
    return (std::min) (m_liveRow, m_cfg.viewportRows - 1);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::Reset
//
////////////////////////////////////////////////////////////////////////////////

void PrinterViewport::Reset ()
{
    m_liveRow         = 0;
    m_liveRowAtScroll = 0;
    m_lastScrollMs    = 0;
    m_following       = true;
}
