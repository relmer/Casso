#pragma once

#include "Pch.h"

#include "Widgets/HexView/DxuiHexViewTypes.h"
#include "Widgets/DxuiScrollbar.h"

class DxuiHexView;
struct DxuiMouseEvent;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiHexScroll
//
//  Scrolling: the top row and the left edge, the two scrollbars and their
//  drags, the wheel, and bringing a byte or the caret into view.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiHexScroll
{
public:
    explicit DxuiHexScroll (DxuiHexView & view);

    uint64_t  GetTopRow         () const { return m_topRow; }
    int       GetLeftPx         () const { return m_leftPx; }
    void      SetTopRow         (uint64_t row);
    void      SetLeftPx         (int leftPx);
    void      ScrollRows        (int64_t delta);
    void      EnsureByteVisible (uint64_t offset);

    //  Scrolls sideways the least that puts the caret's cell in the active
    //  column in view.
    void      KeepCaretInView   ();

    void                            SyncScrollbar    ();
    void                            ScrollToBarPos   ();
    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu (POINT pt);

    //  A press on either bar, the thumb drag that follows and its release, and
    //  the wheel. TryRelease returns whether a thumb was being dragged, and
    //  sets outHandled as the bar's release reports.
    bool  OnPress    (const DxuiMouseEvent & ev);
    bool  OnDrag     (const DxuiMouseEvent & ev);
    bool  TryRelease (bool & outHandled);
    bool  OnWheel    (const DxuiMouseEvent & ev);

    bool  IsDragging () const { return m_vertScroll.IsDragging() || m_horzScroll.IsDragging(); }

private:
    friend class DxuiHexView;

    DxuiHexView  & m_view;

    uint64_t       m_topRow    = 0;
    int            m_leftPx    = 0;
    float          m_wheelRows = 0.0f;   // a touchpad's part notches, until they make a row
    float          m_wheelPx   = 0.0f;
    DxuiScrollbar  m_vertScroll;
    DxuiScrollbar  m_horzScroll;
};
