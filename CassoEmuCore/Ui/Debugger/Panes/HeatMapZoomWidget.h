#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  HeatMapZoomWidget
//
//  The heat map's zoom, in the map's bottom-right corner: a button that shows
//  the zoom as a percentage of the starting cell size, and, opened by a click
//  on it, a panel above it with a slider from the smallest cell to the
//  largest and a Reset button. The slider runs on the cell size's logarithm,
//  so each step along it scales the cells by the same factor.
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

    static constexpr int  kButtonWidthDip  = 48;
    static constexpr int  kButtonHeightDip = 20;
    static constexpr int  kPanelWidthDip   = 220;
    static constexpr int  kPanelHeightDip  = 58;
    static constexpr int  kMarginDip       = 6;
    static constexpr int  kPadDip          = 10;
    static constexpr int  kRowDip          = 20;
    static constexpr int  kResetWidthDip   = 56;
    static constexpr int  kThumbDip        = 6;
    static constexpr int  kPercent         = 100;

    //  The tip over the button.
    static constexpr const wchar_t * kpszTip = L"Zoom\nCtrl+wheel over the map zooms in and out about the pointer. Click for a slider and Reset.";

    //  The zoom as a percentage of the starting cell size, "300%".
    static int           GetPercent      (int cellPx, int startCellPx);
    static std::wstring  GetPercentLabel (int cellPx, int startCellPx);

    //  How far along the slider, from 0 to 1, a cell size sits between one
    //  pixel and the largest, and the cell size a place along it stands for.
    static float  GetPosition         (int cellPx, int maxCellPx);
    static int    GetCellPxAtPosition (float position, int maxCellPx);

    //  Placed in the bottom-right corner of corner, its panel kept inside
    //  the view's bounds.
    void  Place (const RECT & corner, const RECT & bounds, const DxuiDpiScaler & scaler);

    RECT  GetButtonRect () const { return m_button; }
    RECT  GetPanelRect  () const;
    RECT  GetTrackRect  () const;
    RECT  GetResetRect  () const;

    bool  IsOpen      () const { return m_isOpen; }
    void  SetOpen     (bool open);
    bool  IsDragging  () const { return m_isDragging; }
    void  SetDragging (bool dragging) { m_isDragging = dragging && m_isOpen; }

    Hit   HitTest     (POINT point) const;

    //  The cell size the point stands for along the track, the ends taken
    //  past either end.
    int   GetCellPxAt (POINT point, int maxCellPx) const;

    void  Paint (IDxuiTextRenderer & text, const IDxuiTheme & theme, int cellPx, int startCellPx, int maxCellPx) const;

private:
    static bool  Contains (const RECT & rect, POINT point);
    static void  PaintBox (IDxuiTextRenderer & text, const RECT & rect, uint32_t fill, uint32_t edge);

    DxuiDpiScaler  m_scaler;
    RECT           m_button     = {};
    RECT           m_bounds     = {};
    bool           m_isOpen     = false;
    bool           m_isDragging = false;
};
