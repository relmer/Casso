#pragma once

#include "Pch.h"
#include "DxuiScrollbar.h"
#include "DxuiTextViewInterfaces.h"

class   DxuiTextLayout;
class   IDxuiPainter;
class   IDxuiTheme;
struct  DxuiMouseEvent;
struct  DxuiKeyEvent;
struct  DxuiPopupMenuItem;





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTextScroll
//
//  A text view's scrolling: the top line and the left column, the
//  vertical and horizontal bars, the wheel, and the scrolling keys.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiTextScroll : public IDxuiTextViewport
{
public:
    DxuiTextScroll (const DxuiTextLayout & layout, const IDxuiTextGeometry & geometry) : m_layout (layout), m_geometry (geometry) {}

    int   GetTopLine             () const override     { return m_topLine; }
    int   GetLeftColumn          () const override     { return m_leftColumn; }
    int   GetLineCap             () const override;
    bool  IsScrollbarVisible     () const override;
    bool  IsHorzScrollbarVisible () const override     { return m_horzVisible; }
    void  ScrollLines            (int delta) override  { SetTopLine (m_topLine + delta); }
    void  ScrollColumns          (int delta) override  { SetLeftColumn (m_leftColumn + delta); }

    void  SetTopLine                 (int line);
    void  SetLeftColumn              (int column);
    void  ResetTopLine               ()              { m_topLine = 0; }
    void  ResetLeftColumn            ()              { m_leftColumn = 0; }
    void  SetHorzScrollbarVisible    (bool visible)  { m_horzVisible = visible; }
    void  SetVerticalScrollbarHidden (bool hidden)   { m_vertBarHidden = hidden; }
    bool  IsDragging                 () const        { return m_vertScroll.IsDragging() || m_horzScroll.IsDragging(); }

    //  Whether a point is on a scrollbar, and the hover that widens it.
    bool  IsOverScrollbar   (POINT pt) const { return IsOverVertScrollbar (pt) || IsOverHorzScrollbar (pt); }
    bool  SetScrollbarHover (POINT pt);
    bool  TickScrollbars    (int64_t nowMs);

    std::vector<DxuiPopupMenuItem>  GetScrollbarMenu (POINT pt);

    void  Paint         (IDxuiPainter & painter, const IDxuiTheme & theme);
    bool  OnWheel       (const DxuiMouseEvent & ev);
    bool  TryStartDrag  (POINT pt, bool & outHandled);
    bool  TryDrag       (POINT pt, bool & outHandled);
    bool  TryEndDrag    (bool & outHandled);
    bool  OnKey         (const DxuiKeyEvent & ev);

    static constexpr int  kWheelLines   = 3;
    static constexpr int  kWheelColumns = 6;

private:
    bool  IsOverVertScrollbar (POINT pt) const { return IsScrollbarVisible() && !m_vertBarHidden && m_vertScroll.HitTest (pt.x, pt.y); }
    bool  IsOverHorzScrollbar (POINT pt) const { return m_horzVisible && m_horzScroll.HitTest (pt.x, pt.y); }
    void  SyncScrollbar       ();

    const DxuiTextLayout     & m_layout;
    const IDxuiTextGeometry  & m_geometry;
    int                        m_topLine       = 0;
    int                        m_leftColumn    = 0;
    bool                       m_horzVisible   = false;
    bool                       m_vertBarHidden = false;
    float                      m_wheelLines    = 0.0f;   // a touchpad's part notches, until they make a line
    float                      m_wheelColumns  = 0.0f;   // and until they make a column
    DxuiScrollbar              m_vertScroll;
    DxuiScrollbar              m_horzScroll;
};
