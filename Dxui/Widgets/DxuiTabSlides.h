#pragma once

#include "Pch.h"

class DxuiTabStrip;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTabSlides
//
//  The gap a dragged tab opens in the strip, for one of the strip's tabs or
//  one dragged in from another window, and each tab's and the + button's slide
//  from where it was drawn to where it is going.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTabSlides
{
public:
    using ClockFn = std::function<int64_t ()>;

    explicit DxuiTabSlides (DxuiTabStrip & strip);

    void  SetIncomingDrag      (int x, int widthPx);
    void  ClearIncomingDrag    ();
    int   GetIncomingIndex     () const { return m_incoming ? m_gap : -1; }
    void  SetClock             (ClockFn fn) { m_clock = std::move (fn); }
    void  SetAnimationsEnabled (bool on) { m_animate = on; }
    bool  IsSliding            () const;

    int64_t  GetNowMs       () const;
    float    GetSlideLeft   (size_t item) const;
    void     RetargetSlides ();
    void     SnapSlides     ();
    void     UpdateDragGap  ();

private:
    friend class DxuiTabStrip;

    //  One tab's slide, or the + button's, from where it was drawn to where
    //  it is going, in the strip's content coordinates.
    struct Slide
    {
        float    from    = 0.0f;
        float    to      = 0.0f;
        int64_t  startMs = 0;
    };

    DxuiTabStrip  & m_strip;

    int                 m_gap           = -1;      // the slot the gap is open at, among the tabs not dragged
    bool                m_incoming      = false;
    int                 m_incomingX     = 0;
    int                 m_incomingWidth = 0;
    std::vector<Slide>  m_slides;                  // one per tab, then the + button
    ClockFn             m_clock;
    bool                m_animate       = true;
};