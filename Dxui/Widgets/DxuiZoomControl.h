#pragma once

#include "Pch.h"

#include "Core/DxuiPanel.h"
#include "Widgets/DxuiButton.h"
#include "Widgets/DxuiSlider.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl
//
//  A window's zoom as a percentage, for the right end of its status bar.
//  Clicking the percentage makes a slider visible between a "-" and a "+"
//  button to its left, and clicking it again hides them. The window owns the
//  zoom: a change made on the control goes to the callback, and SetZoom sets
//  the zoom the control displays.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiZoomControl : public DxuiPanel
{
public:
    using ZoomFn = std::function<void (double zoom)>;

    //  The control's width with the slider visible, from its right edge.
    static constexpr int     kWidthDip  = 270;
    static constexpr double  kMinZoom   = 0.5;
    static constexpr double  kMaxZoom   = 3.0;

    DxuiZoomControl();

    DxuiZoomControl             (const DxuiZoomControl &) = delete;
    DxuiZoomControl & operator= (const DxuiZoomControl &) = delete;

    void    SetZoom    (double zoom);
    double  GetZoom    () const { return m_zoom; }
    void    SetOnZoom  (ZoomFn fn) { m_onZoom = std::move (fn); }
    bool    IsSliderShown () const { return m_isSliderShown; }

    //  Whether the slider's thumb is being dragged. When the drag ends, the
    //  callback is called again with the zoom it ended on.
    bool    IsDragging    () const { return m_slider->IsDragging(); }

    //  The zoom a number of steps from `zoom` through the standard stops, as
    //  a browser zooms: -1 for the next stop down, 1 for the next one up.
    static double  StepZoom      (double zoom, int steps);
    static std::wstring  FormatZoom (double zoom);

    void  Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler) override;

private:
    void  ChangeZoom (double zoom);

    DxuiButton     * m_zoomOut       = nullptr;
    DxuiSlider     * m_slider        = nullptr;
    DxuiButton     * m_zoomIn        = nullptr;
    DxuiButton     * m_percent       = nullptr;
    DxuiDpiScaler    m_scaler;
    double           m_zoom          = 1.0;
    bool             m_isSliderShown = false;
    ZoomFn           m_onZoom;
};
