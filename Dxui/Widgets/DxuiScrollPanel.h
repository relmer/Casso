#pragma once

#include "Pch.h"
#include "Core/DxuiPanel.h"
#include "Widgets/DxuiScrollbar.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiScrollPanel
//
//  A panel that shows its children through a viewport and scrolls them
//  vertically when they run past it, as a browser scrolls an overflowing
//  box. The owner places each child where it sits with the panel scrolled
//  to its top, then lays the panel out in its viewport; the panel moves the
//  children by the scroll position, clips their painting and their input to
//  the viewport, and draws a scrollbar down the viewport's right edge while
//  they do not fit. The owner leaves that strip free.
//
//  A wheel turn over the viewport scrolls the panel, and passes on once the
//  panel can scroll no further that way, so the page around it scrolls
//  instead. The panel is a tab group: Tab visits every child in order, those
//  scrolled out of view included, and one focused from the keyboard is
//  scrolled into view.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiScrollPanel : public DxuiPanel
{
public:
    static constexpr int  kScrollbarWidthDip = 10;

    DxuiScrollPanel();

    // The scrollbar reports to this panel, so the panel stays where it was
    // built.
    DxuiScrollPanel             (const DxuiScrollPanel &) = delete;
    DxuiScrollPanel & operator= (const DxuiScrollPanel &) = delete;

    // Where a child sits with the panel scrolled to its top, in client
    // pixels. The next Layout moves it by the scroll position.
    void     PlaceChild          (IDxuiControl & child, const RECT & rectPx);

    // How far one wheel notch or one scrollbar arrow click moves the
    // children, in pixels: a row, for a list of rows.
    void     SetLineStepPx       (int stepPx) { m_lineStepPx = std::max (stepPx, 1); }

    bool     IsScrollable        () const { return m_isScrollable; }
    int      GetScrollPosPx      () const { return m_scrollPosPx; }
    int      GetMaxScrollPosPx   () const;
    int      GetContentHeightPx  () const { return m_contentPx; }
    RECT     GetViewportPx       () const { return m_viewportPx; }
    void     SetScrollPosPx      (int posPx);

    void     Layout              (const RECT          & viewportPx,
                                  const DxuiDpiScaler & scaler) override;
    void     Paint               (IDxuiPainter        & painter,
                                  IDxuiTextRenderer   & text,
                                  const IDxuiTheme    & theme) override;
    bool     OnMouse             (const DxuiMouseEvent & ev) override;
    LPCWSTR  GetCursorForPoint   (POINT clientPx) const override;
    bool     IsPointClipped      (POINT clientPx) const override;
    bool     IsTabGroup          () const override { return true; }
    void     RevealDescendant    (const IDxuiControl & descendant) override;

    static int  ClampScrollPos       (int posPx, int contentPx, int viewportPx);
    static int  GetScrollPosToReveal (int posPx, const RECT & targetPx, const RECT & viewportPx);

private:
    // A child and where it sits with the panel scrolled to its top.
    struct Placement
    {
        IDxuiControl  * child  = nullptr;
        RECT            rectPx = {};
    };

    int      ComputeContentHeightPx ()               const;
    int      ComputeInsetPx         ()               const;
    void     ApplyScrollPos         ();
    void     ConfigureScrollbar     ();
    bool     IsInViewport           (POINT clientPx) const;

    std::vector<Placement>  m_placements;
    DxuiScrollbar           m_scrollbar;
    DxuiDpiScaler           m_scaler;
    RECT                    m_viewportPx   = {};
    int                     m_lineStepPx   = 1;
    int                     m_contentPx    = 0;
    int                     m_insetPx      = 0;
    int                     m_scrollPosPx  = 0;
    bool                    m_isScrollable = false;
};
