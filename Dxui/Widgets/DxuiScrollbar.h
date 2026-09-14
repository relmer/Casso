#pragma once

#include "Pch.h"

class IDxuiPainter;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollInfo
//
//  Thin derivation of the Win32 SCROLLINFO whose constructor manages the
//  cbSize ABI field, so callers never set it by hand. Layout is identical
//  to SCROLLINFO (no extra members, no virtuals), so a DxuiScrollInfo is
//  pointer-interconvertible with its base and may be passed anywhere a
//  SCROLLINFO is expected.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiScrollInfo : SCROLLINFO
{
    DxuiScrollInfo() : SCROLLINFO{} { cbSize = sizeof (SCROLLINFO); }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollbar
//
//  A single, orientation-parameterized scrollbar: the one 1-D component
//  that both the vertical and horizontal bars of a scrollable view share,
//  replacing the mirror-image copies that used to live inside the view.
//
//  The model is Win32's: a SCROLLINFO range (nMin / nMax), a page extent
//  (nPage), and a position (nPos). Units are the host's own — rows for a
//  row-quantized vertical bar, pixels for a horizontal one — since the
//  geometry math is unit-agnostic. The host lays out the track rectangle
//  (which strip the bar occupies, in widget-relative pixels) and feeds the
//  model; the bar owns arrow / track / thumb geometry, hit-testing, the
//  thumb-drag lifecycle, and painting.
//
//  Interaction reports through onScroll as a Win32 scroll code (SB_LINEUP,
//  SB_LINEDOWN, SB_PAGEUP, SB_PAGEDOWN, SB_THUMBTRACK) plus the resulting
//  position, mirroring a WM_VSCROLL / WM_HSCROLL notification.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiScrollbar
{
public:
    enum class Orientation { Vertical = SB_VERT, Horizontal = SB_HORZ };

    // Resolved geometry, all widget-relative pixels. The thumb keeps a
    // float position / length for sub-pixel smoothness; the arrow and
    // track rects are integer. arrowLess / arrowMore are empty when the
    // bar is too short to host arrow buttons.
    struct Metrics
    {
        bool   visible     = false;
        RECT   bar         = {};
        RECT   arrowLess   = {};
        RECT   arrowMore   = {};
        RECT   track       = {};
        float  thumbStart  = 0.0f;
        float  thumbLength = 0.0f;
    };

    // Configuration. arrowStep is the position delta for an arrow-button
    // click (one line); track clicks page by the SCROLLINFO page extent.
    void     Configure (Orientation orientation, int thicknessPx, int minThumbPx, int arrowStepPx);
    void     SetTrack (const RECT & trackPx)  { m_track = trackPx; }
    void     SetScrollInfo (const SCROLLINFO & info);
    void     GetScrollInfo (SCROLLINFO & info) const;
    void     SetScrollPos (int pos);
    int      GetScrollPos() const             { return m_pos; }
    int      GetMaxScrollPos() const;

    // Queries.
    bool     IsVisible() const;
    bool     IsDragging() const               { return m_dragging; }
    Metrics  GetMetrics() const;
    bool     HitTest (int xPx, int yPx) const;

    // Input (widget-relative px). OnMouseDown classifies the press
    // (arrow / track / thumb) and acts; OnMouseMove drives an in-progress
    // thumb drag; OnMouseUp ends it. Each returns true when consumed and
    // fires onScroll if the position changed.
    bool     OnMouseDown (int xPx, int yPx);
    bool     OnMouseMove (int xPx, int yPx);
    bool     OnMouseUp();

    //  For a host that runs the thumb drag itself: the puck's distance along
    //  the track while the drag lasts, or nothing once it ends. The puck is
    //  drawn there and stays wide, whatever position the host reports.
    void     SetDragOffset (std::optional<float> offsetPx) { m_dragging = offsetPx.has_value(); m_dragOffset = offsetPx.value_or (0.0f); }

    void     Paint (IDxuiPainter & painter, uint32_t foregroundArgb) const;

    //  Widened at once, as Windows widens a scrollbar the pointer is over: the
    //  puck thickens and the arrows come back. The grab band is the same
    //  either way.
    void     SetExpanded (bool expanded)      { m_expanded = expanded; m_hoverAmount = expanded ? 1.0f : 0.0f; }

    //  SetHover starts the bar widening or narrowing and notes the arrow under
    //  the pointer. Tick carries the change out over a few frames, and reports
    //  whether it is still under way.
    bool     SetHover    (bool over, POINT pt = { -1, -1 });
    bool     Tick        (int64_t nowMs);
    bool     IsExpanded  () const             { return m_expanded; }

    void     SetOnScroll (std::function<void (int sbCode, int pos)> cb)  { m_onScroll = std::move (cb); }

private:
    int    GetContentExtent() const;     // nMax - nMin
    int    GetMainTrackStart() const;    // along the scroll axis
    int    GetMainTrackLength() const;
    int    GetArrowExtent() const;       // 0 when the bar is too short
    float  GetThumbStart() const;
    float  GetThumbLength() const;
    RECT   GetMainRect (int mainStart, int mainExtent) const;
    void   NotifyPos (int sbCode, int newPos);
    void   PaintThumb (IDxuiPainter & painter, float x, float y, float w, float h, uint32_t argb) const;
    void   PaintArrow (IDxuiPainter & painter, const RECT & rect, bool less, uint32_t argb) const;

    Orientation                            m_orientation = Orientation::Vertical;
    int                                    m_thicknessPx = 0;
    int                                    m_minThumbPx  = 0;
    int                                    m_arrowStepPx = 1;
    RECT                                   m_track       = {};
    int                                    m_min         = 0;
    int                                    m_max         = 0;
    int                                    m_page        = 0;
    int                                    m_pos         = 0;
    bool                                   m_dragging    = false;
    bool                                   m_expanded    = false;
    float                                  m_hoverAmount = 0.0f;
    int64_t                                m_lastTickMs  = 0;
    int                                    m_restThumbPx = 3;
    float                                  m_dragGrab    = 0.0f;
    float                                  m_dragOffset  = 0.0f;
    int                                    m_hoverArrow  = 0;
    std::function<void (int sbCode, int pos)>  m_onScroll;
};
