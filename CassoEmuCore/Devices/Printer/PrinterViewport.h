#pragma once

#include "Pch.h"

#include "Devices/Printer/PrinterTypes.h"




////////////////////////////////////////////////////////////////////////////////
//
//  PrinterViewport
//
//  The live-preview FOLLOW-MODE adapter (FR-033). The scroll position itself
//  now lives in the reusable DxuiPanZoom component (which owns the eased panY,
//  input, and clamping); this class is the printer-specific policy layer on top
//  of it: it tracks the print head, decides when the view should ride the live
//  row versus stay where the user parked it, and reports the legal scroll
//  bounds panZoom clamps to. Pure and clock-injected so the follow / snap
//  policy stays unit-tested.
//
//  Model: the view is a window ~one page tall over the fanfold strip, anchored
//  by its BOTTOM row (the printer sits at the bottom of the panel; paper feeds
//  upward). Two modes:
//
//   - Following (default): the bottom tracks the newest printed row, so the
//     view rides the print head and fresh rows appear at the bottom. The panel
//     drives panZoom's panY target to LiveRow each frame.
//   - Scrolled: the user panned away (NotifyUserScroll); the panel stops
//     driving the target, so panZoom holds the parked position. The legal
//     range runs from topClearanceRows of blank feed past the top of the
//     paper (MinBottomRow -- so row 0 clears the 3D curl and reads flat)
//     forward to the live row itself (MaxBottomRow -- the bottom is LOCKED to
//     the last printed row, never scrolling blank in past it).
//
//  Snap-to-live: after snapDelayMs of scroll idle the view returns to
//  Following -- but only once the live row has ADVANCED past where it stood
//  at the last scroll. A finished print stays where the user put it; an
//  active print reclaims the view ("return to the currently printing row").
//
////////////////////////////////////////////////////////////////////////////////

class PrinterViewport
{
public:
    struct Config
    {
        int       viewportRows     = PrinterGrid::kPageRows;       // ~1 page tall (FR-033)
        int64_t   snapDelayMs      = 2000;                         // idle gap before snap-to-live
        int       topClearanceRows = PrinterGrid::kPageRows / 3;   // extra back-scroll so row 0 clears the 3D curl
    };

    // Bottom-anchored native-row span for the renderer (inclusive). The panel
    // fills this from panZoom's eased bottom row each frame.
    struct Span
    {
        int   firstRow = 0;   // inclusive
        int   lastRow  = 0;   // inclusive (the view's bottom row)
    };

    explicit PrinterViewport (const Config & cfg = Config ());

    // The newest printed row (monotonic; lesser values are ignored).
    void   Advance       (int liveRow);

    // The user took manual control of the scroll (wheel / drag / arrow keys):
    // leave Following so the view holds where they park it, and arm the
    // snap-back clock.
    void   NotifyUserScroll (int64_t nowMs);

    // Per-frame clock tick: performs the idle snap back to the live row.
    void   Tick          (int64_t nowMs);

    bool   FollowingLive () const { return m_following; }
    int    LiveRow       () const { return m_liveRow; }
    int    ViewportRows  () const { return m_cfg.viewportRows; }

    // Legal scroll range for the bottom row, the bounds panZoom clamps panY to.
    // MinBottomRow: furthest back -- a full viewport against the top of the
    // paper plus topClearanceRows so row 0 clears the 3D curl, unless the strip
    // is still shorter than that (then it pins to the live row). MaxBottomRow:
    // furthest forward -- the live row itself (the bottom is locked there).
    int    MinBottomRow  () const;
    int    MaxBottomRow  () const { return m_liveRow; }

    // Forget history (machine switch / discard): back to Following at row 0.
    void   Reset         ();

private:
    Config    m_cfg;
    int       m_liveRow         = 0;
    int       m_liveRowAtScroll = 0;    // live row when the user last scrolled
    int64_t   m_lastScrollMs    = 0;
    bool      m_following       = true;
};
