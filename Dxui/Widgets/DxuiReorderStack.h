#pragma once

#include "Pch.h"

#include "Core/DxuiPanel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiReorderStack
//
//  A column of rows of one height, each drawn in a box with a gripper at its
//  leading edge and the caller's control in the rest. The first rows, up to
//  the movable count, are reordered by dragging a gripper: the row lifts out
//  and follows the pointer, and the other movable rows slide apart to open a
//  gap where it would drop, as the tab strip's tabs do. The rows past the
//  movable count are drawn dimmed, with no gripper, and don't move.
//
//  The caller has the order, not the stack. A drop calls the reorder
//  function with the row's index and the index it dropped at; the caller
//  moves its data and fills each row's control again, and the dropped row
//  slides from where it was let go to its place. Each row's control is laid out at its row's
//  place on every paint, so it moves with its row.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiReorderStack : public DxuiPanel
{
public:
    using ReorderFn = std::function<void (size_t from, size_t to)>;
    using ClockFn   = std::function<int64_t ()>;

    static constexpr int  kSlideMs     = 160;
    static constexpr int  kGripDip     = 22;
    static constexpr int  kRowGapDip   = 6;
    static constexpr int  kPaddingDip  = 6;

    //  The rows' controls, in order, adopted so they take input and focus.
    void  SetRows         (const std::vector<IDxuiControl *> & rows);
    void  SetMovableCount (size_t count);
    void  SetRowHeightDip (int heightDip) { m_rowHeightDip = heightDip; }
    void  SetOnReorder    (ReorderFn reorder) { m_reorder = std::move (reorder); }
    void  SetClock        (ClockFn clock) { m_clock = std::move (clock); }

    void  SetAnimationsEnabled (bool on) { m_animate = on; }

    //  The height of every row and the gaps between them, in pixels.
    int   MeasureHeightPx (const DxuiDpiScaler & scaler) const;

    //  Whether a row is being dragged or is sliding, so the host repaints.
    bool  IsAnimating () const;

    //  The row being dragged, or -1.
    int   GetDraggedRow () const { return m_dragged; }

    void     Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void     Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool     OnMouse           (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint (POINT clientPx) const override;

private:
    //  A row's slide from where it was drawn to its place, in pixels from
    //  the stack's top.
    struct Slide
    {
        float    from    = 0.0f;
        float    to      = 0.0f;
        int64_t  startMs = 0;
    };

    static constexpr int    kGripDots       = 3;
    static constexpr float  kDotDip         = 1.5f;
    static constexpr float  kDotPitchDip    = 5.0f;
    static constexpr float  kCornerDip      = 4.0f;
    static constexpr float  kEdgeDip        = 1.0f;
    static constexpr float  kDraggedEdgeDip = 2.0f;
    static constexpr float  kDimAlpha       = 0.55f;

    int64_t  GetNowMs        () const;
    int      GetPitchPx      () const;
    int      GetSlotTop      (size_t slot) const;
    float    GetRowTop       (size_t row) const;
    RECT     GetRowRect      (float top) const;
    RECT     GetGripRect     (const RECT & row) const;
    int      FindGripRow     (POINT point) const;
    int      GetGapSlot      () const;
    void     RetargetSlides  ();
    void     SnapSlides      ();
    void     LayoutRows      ();
    void     PaintRow        (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme, size_t row, float top) const;

    static void  PushClip (IDxuiPainter & painter, IDxuiTextRenderer & text, const RECT & clip);
    static void  PopClip  (IDxuiPainter & painter, IDxuiTextRenderer & text);

    std::vector<IDxuiControl *>  m_rows;
    std::vector<Slide>           m_slides;
    size_t                       m_movable      = 0;
    int                          m_rowHeightDip = 40;
    RECT                         m_bounds       = {};
    DxuiDpiScaler                m_scaler;
    ReorderFn                    m_reorder;
    ClockFn                      m_clock;
    bool                         m_animate      = true;
    int                          m_dragged      = -1;
    int                          m_gapSlot      = -1;
    int                          m_grabOffset   = 0;     // the pointer's distance below the dragged row's top
    int                          m_pointerY     = 0;
};
