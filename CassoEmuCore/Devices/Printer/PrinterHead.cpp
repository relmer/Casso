#include "Pch.h"

#include "Devices/Printer/PrinterHead.h"




////////////////////////////////////////////////////////////////////////////////
//
//  FormFeedDurationSec
//
//  The page-feed sound grain a feed of `rows` selects: short below 1/3 of a
//  page, medium below 2/3, long beyond. The feed rate (rows / duration) then
//  stops the paper exactly when the grain ends.
//
////////////////////////////////////////////////////////////////////////////////

double PrinterHead::FormFeedDurationSec (int rows)
{
    double   frac = (double) rows / (double) PrinterGrid::kPageRows;
    int      slot = (frac < 1.0 / 3.0) ? 0 : (frac < 2.0 / 3.0) ? 1 : 2;

    return kPageFeedGrainSec[slot];
}




////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
//  Parks the head at `parkRow` with an empty timeline. The frontier and mask
//  start at the park row so nothing below it reads as printed.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterHead::Reset (int parkRow)
{
    m_pending.clear();
    m_phase          = Phase::Idle;
    m_headRow        = (double) parkRow;
    m_headCol        = 0.0;
    m_sweepWidth     = 0.0;
    m_feedTarget     = 0.0;
    m_feedRate       = kFeedRowsPerSec;
    m_frontier       = (double) parkRow;
    m_sweepMaskTop   = (double) parkRow;
    m_sweepPaintedTo = 0.0;
    m_sweepColor     = 0;
    m_sweepLtr       = true;
    m_nextLtr        = true;
    m_carriageCol    = 0;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Queue
//
//  Appends a drain's head-motion events (passes + feeds) to the timeline; other
//  event types carry no head motion and are ignored.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterHead::Queue (const vector<PrinterEvent> & events)
{
    for (const PrinterEvent & ev : events)
    {
        if (ev.type == PrinterEventType::HeadBurst ||
            ev.type == PrinterEventType::LineFeed  ||
            ev.type == PrinterEventType::FormFeed)
        {
            m_pending.push_back (ev);
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PendingSeconds
//
//  Print time still queued: the remainder of the in-progress motion plus every
//  event waiting in the timeline, each at its own speed.
//
////////////////////////////////////////////////////////////////////////////////

double PrinterHead::PendingSeconds() const
{
    double   seconds = 0.0;

    if (m_phase == Phase::Sweeping)
    {
        seconds += (m_sweepWidth - m_headCol) / kCarriageDotsPerSec;
    }
    else if (m_phase == Phase::Feeding)
    {
        seconds += (m_feedTarget - m_headRow) / (m_feedRate > 0.0 ? m_feedRate : kFeedRowsPerSec);
    }

    for (const PrinterEvent & ev : m_pending)
    {
        if (ev.type == PrinterEventType::HeadBurst)
        {
            seconds += (double) (ev.toDot + 1) / kCarriageDotsPerSec;
        }
        else if (ev.type == PrinterEventType::FormFeed)
        {
            seconds += (ev.rows > 0) ? FormFeedDurationSec (ev.rows) : 0.0;
        }
        else if (ev.type == PrinterEventType::LineFeed)
        {
            seconds += (double) ev.rows / kFeedRowsPerSec;
        }
    }

    return seconds;
}




////////////////////////////////////////////////////////////////////////////////
//
//  Advance
//
//  Replays `timeSec` of print time along the event timeline. Sweeping crosses
//  the current burst's printed width at the carriage speed (painting its ink up
//  to the head and tracking the carriage glyph column); the pass completes and
//  the next event is popped. Feeding slews the platen toward the feed target at
//  the feed speed. Idle pops the next event: a HeadBurst starts a sweep, snapping
//  the platen to the pass row and flipping direction (bidirectional print); a
//  LineFeed / FormFeed starts a slew.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterHead::Advance (double timeSec, const PrintRaster & built, PrintRaster & presented)
{
    int   guard = 0;

    while (timeSec > 0.0 && guard++ < 8192)
    {
        if (m_phase == Phase::Sweeping)
        {
            double   remaining = m_sweepWidth - m_headCol;
            double   sweepTime = remaining / kCarriageDotsPerSec;

            if (sweepTime > timeSec)
            {
                m_headCol += kCarriageDotsPerSec * timeSec;
                timeSec    = 0.0;
            }
            else
            {
                timeSec  -= sweepTime;
                m_headCol = m_sweepWidth;
                m_phase   = Phase::Idle;   // pass complete
            }

            PaintPresented (m_sweepPaintedTo, m_headCol, built, presented);   // lay this pass's ink up to the head
            m_sweepPaintedTo = m_headCol;

            // Track the carriage glyph: a right-to-left pass mirrors around THIS
            // line's printed width (logic seeking), not the full platen, so a
            // short text line sweeps back over its own ink. Captured here so it
            // holds at the pass end and parks there through the following feed.
            m_carriageCol = m_sweepLtr ? (int) m_headCol
                                       : (int) (m_sweepWidth - m_headCol);
        }
        else if (m_phase == Phase::Feeding)
        {
            double   remaining = m_feedTarget - m_headRow;
            double   feedTime  = remaining / m_feedRate;

            if (remaining <= 0.0)
            {
                m_headRow = m_feedTarget;
                m_phase   = Phase::Idle;
            }
            else if (feedTime > timeSec)
            {
                m_headRow += m_feedRate * timeSec;
                timeSec    = 0.0;
            }
            else
            {
                timeSec  -= feedTime;
                m_headRow = m_feedTarget;
                m_phase   = Phase::Idle;   // feed complete
            }
        }
        else   // Idle: start the next queued motion
        {
            PrinterEvent   ev;

            if (m_pending.empty())
            {
                break;   // nothing queued -- head parked
            }

            ev = m_pending.front();
            m_pending.pop_front();

            if (ev.type == PrinterEventType::HeadBurst)
            {
                double   width = (double) (ev.toDot + 1);

                if (width < 1.0)                                  { width = 1.0; }
                if (width > (double) PrinterGrid::kDotsPerRow)    { width = (double) PrinterGrid::kDotsPerRow; }

                // Mask only the new ink: passes overlap the previous band by a few
                // rows (feed < band height) for a gapless join, and those already-
                // printed rows must not blank when this pass starts, so the mask
                // top never rises above the monotonic print frontier.
                m_headRow        = (double) ev.row;                                // platen snaps to the pass row
                m_sweepMaskTop   = (std::max) ((double) ev.row, m_frontier);
                m_frontier       = (std::max) (m_frontier, (double) ev.row + PrinterGrid::kPinBandRows);
                m_sweepWidth     = width;
                m_headCol        = 0.0;
                m_sweepColor     = (Byte) ev.color;                                // primary this pass lays
                m_sweepPaintedTo = 0.0;
                m_sweepLtr       = m_nextLtr;
                m_nextLtr        = !m_nextLtr;                                      // alternate each pass (bidirectional)
                m_phase          = Phase::Sweeping;
            }
            else if (ev.type == PrinterEventType::LineFeed ||
                     ev.type == PrinterEventType::FormFeed)
            {
                m_feedTarget = m_headRow + (double) ev.rows;

                if (ev.type == PrinterEventType::FormFeed && ev.rows > 0)
                {
                    // Match the paper to its sound: finish exactly at the grain end.
                    m_feedRate = (double) ev.rows / FormFeedDurationSec (ev.rows);
                }
                else
                {
                    m_feedRate = kFeedRowsPerSec;
                }

                m_phase = Phase::Feeding;
            }
            // other event types carry no head motion -- already consumed
        }
    }
}




////////////////////////////////////////////////////////////////////////////////
//
//  PaintPresented
//
//  Lays the current sweep's primary into the presented ("wet ink") layer across
//  the carriage travel [fromP, toP] (progress dots). The band is struck only
//  where the built strip already carries this pass's colour, so an overprint
//  accrues into a composite (red pass -> red; blue return -> red|blue = purple)
//  and the same render-time mixing that produces the finished image plays out
//  live.
//
////////////////////////////////////////////////////////////////////////////////

void PrinterHead::PaintPresented (double fromP, double toP,
                                  const PrintRaster & built, PrintRaster & presented)
{
    if (m_sweepColor == 0 || toP <= fromP)
    {
        return;
    }

    int   row0 = (int) m_headRow;
    int   row1 = row0 + PrinterGrid::kPinBandRows - 1;
    int   c0   = 0;
    int   c1   = 0;

    // Map swept progress to physical carriage columns: a right-to-left pass lays
    // from the right edge inward, so its columns mirror around the sweep width.
    if (m_sweepLtr)
    {
        c0 = (int) fromP;
        c1 = (int) toP;
    }
    else
    {
        c0 = (int) (m_sweepWidth - toP);
        c1 = (int) (m_sweepWidth - fromP);
    }

    c0 = (std::max) (0, c0);
    c1 = (std::min) (PrinterGrid::kDotsPerRow - 1, c1);

    for (int r = row0; r <= row1; r++)
    {
        for (int c = c0; c <= c1; c++)
        {
            Byte   lay = (Byte) (built.CellAt (c, r) & m_sweepColor);

            if (lay != 0)
            {
                presented.Strike (c, r, (InkPrimary) lay);
            }
        }
    }
}
