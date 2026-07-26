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
//  The furthest back the bottom may scroll: a full viewport against the top of
//  the paper PLUS topClearanceRows of blank feed, so the top of page 1 (row 0)
//  scrolls clear of the 3D curl and reads flat -- unless the strip is still
//  shorter than that, in which case the bottom pins to the live row.
//
////////////////////////////////////////////////////////////////////////////////

int PrinterViewport::MinBottomRow() const
{
    int  furthestBack = (std::max) (0, m_cfg.viewportRows - 1 - m_cfg.topClearanceRows);

    return (std::min) (m_liveRow, furthestBack);
}




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport::Reset
//
////////////////////////////////////////////////////////////////////////////////

void PrinterViewport::Reset()
{
    m_liveRow         = 0;
    m_liveRowAtScroll = 0;
    m_lastScrollMs    = 0;
    m_following       = true;
}
