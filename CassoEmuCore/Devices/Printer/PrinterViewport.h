#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterTypes.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport
//
//  The live-preview viewport state machine (FR-033). Pure and clock-injected
//  so the follow / scroll / snap behavior is unit-tested; the panel feeds it
//  input and asks for the visible native-row span each frame.
//
//  Model: the view is a window ~one page tall over the fanfold strip,
//  anchored by its BOTTOM row (the printer sits at the bottom of the panel;
//  paper feeds upward). Two modes:
//
//   - Following (default): the bottom tracks the newest printed row, so the
//     view rides the print head and fresh rows appear at the bottom.
//   - Scrolled: the user scrolled back to review earlier pages; the bottom is
//     an ABSOLUTE strip row (it does not drift as printing continues). After
//     snapDelayMs without further scrolling, the view snaps back to Following
//     (the "return to the currently printing row" behavior).
//
//  Scrolling forward past the live row also returns to Following, so the two
//  exits are symmetric: catch up by hand, or pause and be brought back.
//
////////////////////////////////////////////////////////////////////////////////

class PrinterViewport
{
public:
    struct Config
    {
        int       viewportRows = PrinterGrid::kPageRows;   // ~1 page tall (FR-033)
        int64_t   snapDelayMs  = 2000;                     // idle gap before snap-to-live
    };

    struct Span
    {
        int   firstRow = 0;   // inclusive
        int   lastRow  = 0;   // inclusive (the view's bottom row)
    };

    explicit PrinterViewport (const Config & cfg = Config ());

    // The newest printed row (monotonic; lesser values are ignored).
    void   Advance       (int liveRow);

    // User scroll input (wheel / touch / arrow keys), in native rows. Negative
    // deltas scroll back toward earlier pages, positive toward the live row.
    void   Scroll        (int deltaRows, int64_t nowMs);

    // Per-frame clock tick: performs the idle snap back to the live row.
    void   Tick          (int64_t nowMs);

    // The visible strip span for the renderer, bottom-anchored and clamped to
    // the strip. The span is always exactly min(viewportRows, liveRow+1) tall.
    Span   VisibleSpan   () const;

    bool   FollowingLive () const { return m_following; }
    int    LiveRow       () const { return m_liveRow; }
    int    ViewportRows  () const { return m_cfg.viewportRows; }

    // Forget history (machine switch / discard): back to Following at row 0.
    void   Reset         ();

private:
    int    BottomRow     () const;
    int    MinBottomRow  () const;

    Config    m_cfg;
    int       m_liveRow      = 0;
    int       m_anchorBottom = 0;       // absolute bottom row while Scrolled
    int64_t   m_lastScrollMs = 0;
    bool      m_following    = true;
};
