#pragma once

#include "Pch.h"

#include "Widgets/ListView/DxuiListViewTypes.h"
#include "Widgets/DxuiScrollbar.h"

class DxuiListView;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiListScrollbars
//
//  The list's scrollbars and scrolling: the rows that fit and the
//  topmost one, the vertical and horizontal bars' geometry, hit tests and
//  thumb drags, the wheel, and the height that fits a number of rows.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiListScrollbars
{
public:
    explicit DxuiListScrollbars (DxuiListView & list);

    int   GetVisibleRowCapacity () const;
    int   GetMaxTopRow          () const;
    void  SetTopRow             (int topRow);
    void  MoveTopRow            (int topRow);            // by whole rows, keeping the rows' lift and any hold
    void  ScrollByWheelDelta    (int wheelDelta, int linesPerNotch);
    int   GetWheelLinesPerNotch () const;

    bool             IsScrollbarVisible        () const;
    DxuiListViewTypes::ScrollbarMetrics GetScrollbarGeometry      () const;
    bool             HitTestScrollbarThumb     (int xPx, int yPx) const;
    bool             HitTestScrollbarTrack     (int xPx, int yPx) const;
    bool             HitTestScrollbarArrowUp   (int xPx, int yPx) const;
    bool             HitTestScrollbarArrowDown (int xPx, int yPx) const;
    void             GetPageFromTrackClick        (int yPx);
    void             BeginThumbDrag            (int grabYPx);
    void             UpdateThumbDrag           (int yPx);
    int   GetMaxLeftPx                 () const;
    void  SetLeftPx                    (int leftPx);
    void  ScrollByWheelDeltaHorizontal (int wheelDelta, int pxPerNotch);

    bool              IsHorzScrollbarVisible         () const;
    DxuiListViewTypes::HorzScrollbarMetrics GetHorzScrollbarGeometry       () const;
    bool              HitTestHorzScrollbarThumb      (int xPx, int yPx) const;
    bool              HitTestHorzScrollbarTrack      (int xPx, int yPx) const;
    bool              HitTestHorzScrollbarArrowLeft  (int xPx, int yPx) const;
    bool              HitTestHorzScrollbarArrowRight (int xPx, int yPx) const;
    void              GetPageFromHorzTrackClick         (int xPx);
    void              BeginHorzThumbDrag             (int grabXPx);
    void              UpdateHorzThumbDrag            (int xPx);

    int   GetRequiredRowsForHeightPx (int heightPx) const;
    int   GetRequiredHeightPx        () const;
    static constexpr int      s_kMinThumbPx       = 16;

    // Scrollbar auto-repeat cadence (ms), mirroring typical key-repeat:
    // a longer delay before the first repeat, then a steady interval.
    static constexpr int64_t  s_kScrollRepeatDelayMs    = 400;
    static constexpr int64_t  s_kScrollRepeatIntervalMs = 60;

    //  How far the rows are drawn above their places, and whether they can be.
    int   GetRowShiftPx () const;
    bool  CanLiftRows   () const;

    //  Rows the body shows at least part of: the whole ones, and a cut one.
    int   GetShownRowCount () const;
    DxuiListViewTypes::ScrollLayout ComputeScrollLayout () const;
    //  A press on either bar, the thumb drag that follows and its release,
    //  and the wheel; each returns whether the bars took the event. Tick
    //  repeats a held arrow or track.
    bool    OnPress   (int lx, int ly);
    bool    OnDrag    (int lx, int ly);
    bool    OnRelease ();
    bool    OnWheel   (const DxuiMouseEvent & ev, bool inside);
    void    Tick      (int64_t nowMs);
    void    EndThumbDrag     ();
    void    EndHorzThumbDrag ();
    bool    IsThumbDragging     () const { return m_vertDragging; }
    bool    IsHorzThumbDragging () const { return m_horzDragging; }
    bool    IsInteracting       () const { return m_vertDragging || m_horzDragging || m_scrollRepeat != DxuiListViewTypes::ScrollRepeat::None; }
    void    PaintScrollbar          (IDxuiPainter  & painter,
                                     const DxuiListViewTypes::Palette & pal,
                                     float           x,
                                     float           y) const;
    void    PaintHScrollbar         (IDxuiPainter  & painter,
                                     const DxuiListViewTypes::Palette & pal,
                                     float           x,
                                     float           y) const;
    void    SyncVertScroll          () const;
    void    SyncHorzScroll          () const;

private:
    friend class DxuiListView;

    DxuiListView  & m_list;

    bool                       m_stickyTail        = false;

    // Whether the host opted into sticky-tail behavior. Off by default: a
    // list opens at its top, and following the last row is for live logs.
    // m_stickyTail is re-derived on every resize and row change, and this
    // gate keeps that re-derivation from switching it on for a list that
    // never asked for it.
    bool   m_stickyTailEnabled = false;
    float  m_vertDragGrab      = 0.0f;

    // High-resolution wheel accumulators. Touchpads emit many sub-notch
    // deltas; we bank them and act only when a whole unit is due, so fine
    // scrolls move proportionally instead of snapping a full line/step per
    // event. m_wheelAccumV is in raw WHEEL_DELTA units (acted per notch);
    // m_wheelAccumH is in pixels (acted per whole pixel, for smooth H-scroll).
    int                    m_wheelAccumV        = 0;
    float                  m_wheelAccumH        = 0.0f;
    float                  m_horzDragGrab       = 0.0f;
    int                    m_scrollRepeatXPx    = 0;
    int                    m_scrollRepeatYPx    = 0;
    int64_t                m_scrollRepeatNextMs = 0;
    mutable DxuiScrollbar  m_vertScroll;
    mutable DxuiScrollbar  m_horzScroll;

    //  A thumb being dragged, and an arrow or the track held down.
    bool                             m_vertDragging = false;
    bool                             m_horzDragging = false;
    DxuiListViewTypes::ScrollRepeat  m_scrollRepeat = DxuiListViewTypes::ScrollRepeat::None;
};
