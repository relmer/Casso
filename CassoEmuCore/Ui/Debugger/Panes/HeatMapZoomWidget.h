#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget
//
//  The heat map's zoom, in the map's bottom-right corner: a button that shows
//  the zoom as a percentage of the starting cell size, and, opened by hovering
//  over it, a panel that grows out of it: the slider, from the smallest cell
//  to the largest, across the top row, and on the button's row Reset at the
//  left and the percentage at the right, where the button has it. The slider
//  runs on the cell size's logarithm, so each step along it scales the cells
//  by the same factor. The panel grows out of the button as it opens and
//  shrinks back into it as it closes, over kAnimateMs, its contents revealed
//  by the growing edge rather than moved.
//
//  It is drawn with the text renderer, over the map's picture, and decides
//  nothing about the map: the view places it against the map's rect, hands
//  it the zoom to show, and acts on what a press on it lands on.
//
////////////////////////////////////////////////////////////////////////////////

class HeatMapZoomWidget
{
public:
    //  What a point is over.
    enum class Hit
    {
        None,
        Button,
        Panel,
        Track,
        Reset,
    };

    static constexpr int  kButtonWidthDip  = 84;
    static constexpr int  kButtonHeightDip = 20;
    static constexpr int  kPanelWidthDip   = 220;
    static constexpr int  kMarginDip       = 6;
    static constexpr int  kPadDip          = 10;
    static constexpr int  kRowDip          = 20;
    static constexpr int  kResetWidthDip   = 56;
    static constexpr int  kThumbDip        = 6;
    static constexpr int  kPercent         = 100;

    //  How long the panel takes to open or close.
    static constexpr int64_t  kAnimateMs   = 140;

    //  How far past the button and its open panel the pointer can go before
    //  the panel closes, as the disk inspector's tilt slider does.
    static constexpr int  kNearDip         = 16;

    //  The tip over the button.
    static constexpr const wchar_t * kpszTip = L"Zoom\nCtrl+wheel over the map zooms in and out about the pointer. Click for a slider and Reset.";

    //  The zoom as a percentage of the starting cell size, "Zoom 300%", so the
    //  button reads as a zoom and not as a figure about the map.
    static int           GetPercent      (int cellPx, int startCellPx);
    static std::wstring  GetPercentLabel (int cellPx, int startCellPx);

    //  How far along the slider, from 0 to 1, a cell size sits between one
    //  pixel and the largest, and the cell size a place along it stands for.
    static float  GetPosition         (int cellPx, int maxCellPx);
    static int    GetCellPxAtPosition (float position, int maxCellPx);

    //  Placed in the bottom-right corner of corner, its panel kept inside
    //  the view's bounds.
    void  Place (const RECT & corner, const RECT & bounds, const DxuiDpiScaler & scaler);

    //  Another control's rect as the button, which the widget doesn't draw
    //  while closed; open, the panel grows out of it and draws the label as
    //  that control does: left-aligned labelInsetDip in, in the body face at
    //  labelFontDip, centered on its cap height.
    void  PlaceAtButton (const RECT & button, const RECT & bounds, const DxuiDpiScaler & scaler, float labelInsetDip, float labelFontDip);

    RECT  GetButtonRect () const { return m_button; }
    RECT  GetPanelRect  () const;
    RECT  GetTrackRect  () const;
    RECT  GetResetRect  () const;

    bool  IsOpen      () const { return m_isOpen; }
    void  SetOpen     (bool open);

    //  Whether any of the panel is drawn: it is open, or its closing
    //  animation is running.
    bool  IsShown     () const { return m_isOpen || m_openness > 0.0f; }

    //  Moves the opening or closing along; true while it goes on, so the
    //  host draws again. The panel as drawn at this point, the button's
    //  rect grown toward GetPanelRect by how far open it is.
    bool  Tick              (int64_t nowMs);
    RECT  GetShownPanelRect () const;
    bool  IsDragging        () const { return m_isDragging; }

    //  The pointer over the button opens the panel, unless a press on the
    //  button closed it; then not until the pointer moves off the button.
    void  OnHover     (POINT point);
    void  Toggle      ();
    void  SetDragging (bool dragging) { m_isDragging = dragging && m_isOpen; }

    Hit   HitTest     (POINT point) const;
    bool  IsNear      (POINT point) const;

    //  The cell size the point stands for along the track, the ends taken
    //  past either end.
    int    GetCellPxAt   (POINT point, int maxCellPx) const;
    float  GetPositionAt (POINT point) const;

    void  Paint      (IDxuiTextRenderer & text, const IDxuiTheme & theme, int cellPx, int startCellPx, int maxCellPx) const;
    void  PaintPanel (IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & label, float position) const;

private:
    static bool  Contains         (const RECT & rect, POINT point);
    RECT         GetFullPanelRect () const;
    RECT         GetResetRectIn   (const RECT & panel) const;
    RECT         GetTrackRectIn   (const RECT & panel) const;
    static void  PaintBox         (IDxuiTextRenderer & text, const RECT & rect, uint32_t fill, uint32_t edge);
    void         PaintLabel       (IDxuiTextRenderer & text, const IDxuiTheme & theme, const std::wstring & label) const;

    DxuiDpiScaler  m_scaler;
    RECT           m_button        = {};
    RECT           m_bounds        = {};
    bool           m_isOpen        = false;
    bool           m_isDragging    = false;
    bool           m_isOwnButton   = true;
    bool           m_isHoverHeld   = false;   // a press closed the panel with the pointer on the button
    float          m_labelInset    = 0.0f;    // another control's label: its inset, in DIPs
    float          m_labelFont     = 0.0f;    // and its font size, in DIPs
    float          m_openness      = 0.0f;    // 0 closed to 1 open, as drawn
    float          m_animFrom      = 0.0f;    // the openness the animation started from
    int64_t        m_animStartMs   = -1;      // when it started, -1 for none
    bool           m_isAnimPending = false;   // started by SetOpen, timed from the next Tick
};
