#pragma once

#include "Pch.h"
#include "Core/IDxuiControl.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiSplitter
//
//  A sash between two panes, dragged or moved with the arrow keys.
//
//  THE SPLITTER OWNS A POSITION, NOT THE PANES. Its bounds are the whole area
//  the two panes share; the position is the first pane's extent in DIPs from
//  the start of that area, and the sash sits there. The consumer lays its panes
//  out around GetSashRect whenever OnMoved reports a change, so the splitter
//  never needs to know what it divides.
//
//  Vertical means the sash is a vertical bar dividing left from right, as in
//  Explorer's tree and list; Horizontal divides top from bottom.
//
//  The position is kept inside the limits whenever the extent is known, so
//  neither pane can be dragged below its minimum.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiSplitter : public IDxuiControl
{
public:
    enum class Orientation { Vertical, Horizontal };

    using MovedFn = std::function<void (int positionDip)>;

    DxuiSplitter() { m_focusable = true; }
    ~DxuiSplitter() override = default;

    void  SetOrientation (Orientation orientation) { m_orientation = orientation; }
    void  SetPositionDip (int dip);
    int   GetPositionDip () const { return m_positionDip; }
    void  SetLimitsDip   (int minFirst, int minSecond);
    void  SetOnMoved     (MovedFn fn) { m_onMoved = std::move (fn); }

    Orientation  GetOrientation () const { return m_orientation; }
    bool         IsDragging     () const { return m_dragging; }
    bool         IsHovered      () const { return m_hovered; }

    //  The sash in the same pixels as the bounds.
    RECT  GetSashRect () const;

    void                Layout            (const RECT & boundsDip, const DxuiDpiScaler & scaler) override;
    void                Paint             (IDxuiPainter & painter, IDxuiTextRenderer & text, const IDxuiTheme & theme) override;
    bool                OnMouse           (const DxuiMouseEvent & ev) override;
    bool                OnKey             (const DxuiKeyEvent & ev) override;
    LPCWSTR             GetCursorForPoint (POINT clientPx) const override;
    DxuiAccessibleRole  GetAccessibleRole () const override { return DxuiAccessibleRole::Custom; }
    std::wstring        GetAccessibleName () const override { return L"Splitter"; }

    static constexpr int  kSashDip    = 4;
    static constexpr int  kKeyStepDip = 8;

private:
    //  The bounds' extent along the axis the sash moves, in DIPs; zero before
    //  the first layout.
    int   GetExtentDip () const;
    int   ClampPositionDip (int dip) const;
    bool  IsOverSash (POINT px) const;
    void  MoveTo (int dip);

    Orientation    m_orientation   = Orientation::Vertical;
    int            m_positionDip   = 0;
    int            m_minFirstDip   = 0;
    int            m_minSecondDip  = 0;
    bool           m_dragging      = false;
    bool           m_hovered       = false;
    int            m_dragOffsetPx  = 0;
    MovedFn        m_onMoved;
    DxuiDpiScaler  m_scaler;
};
