#include "Pch.h"

#include "Widgets/DxuiZoomControl.h"
#include "Core/DxuiUnicodeSymbols.h"





static constexpr int     s_kButtonDip   = 26;
static constexpr int     s_kSliderDip   = 140;
static constexpr float   s_kTickPercent = 50.0f;
static constexpr int     s_kPercentDip  = 56;

//  The stops a step moves between, as percentages.
static constexpr int     s_kStops[]     = { 50, 67, 75, 80, 90, 100, 110, 125, 150, 175, 200, 250, 300 };





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::DxuiZoomControl
//
//  The slider runs over the zoom range in whole percents; it and its
//  buttons start hidden.
//
////////////////////////////////////////////////////////////////////////////////

DxuiZoomControl::DxuiZoomControl()
{
    m_zoomOut = CreateChild<DxuiButton> (s_kpszMinusSign);
    m_slider  = CreateChild<DxuiSlider>();
    m_zoomIn  = CreateChild<DxuiButton> (L"+");
    m_percent = CreateChild<DxuiButton> (FormatZoom (m_zoom));

    m_slider->SetRange     (static_cast<float> (kMinZoom * 100), static_cast<float> (kMaxZoom * 100));
    m_slider->SetStep      (10.0f);
    m_slider->SetDragStep  (1.0f);
    m_slider->SetTickInterval (s_kTickPercent);
    m_slider->SetShowValue (false);
    m_slider->SetValue     (100.0f);
    m_slider->SetOnChange  ([this] (float value) { ChangeZoom (value / 100.0); });

    m_zoomOut->SetOnClick ([this] () { ChangeZoom (StepZoom (m_zoom, -1)); });
    m_zoomIn->SetOnClick  ([this] () { ChangeZoom (StepZoom (m_zoom,  1)); });
    m_percent->SetOnClick ([this] () { m_isSliderShown = !m_isSliderShown; Layout (m_boundsDip, m_scaler); });
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::SetZoom
//
//  Sets the zoom the control displays; the callback is not called.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiZoomControl::SetZoom (double zoom)
{
    m_zoom = std::clamp (zoom, kMinZoom, kMaxZoom);
    m_slider->SetValue  (static_cast<float> (m_zoom * 100));
    m_percent->SetLabel (FormatZoom (m_zoom));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::StepZoom
//
//  From between two stops, a step down lands on the lower one and a step up
//  on the higher one.
//
////////////////////////////////////////////////////////////////////////////////

double DxuiZoomControl::StepZoom (double zoom, int steps)
{
    static constexpr double  kNear = 0.5;



    double  percent = zoom * 100;
    int     index   = 0;
    int     count   = static_cast<int> (std::size (s_kStops));



    //  The stop at or just below the zoom, then that many stops on; a step
    //  down from a zoom between stops counts the lower stop as the first.
    while (index + 1 < count && s_kStops[index + 1] <= percent + kNear)
    {
        index++;
    }

    if (steps < 0 && percent > s_kStops[index] + kNear)
    {
        steps++;
    }

    index = std::clamp (index + steps, 0, count - 1);

    return s_kStops[index] / 100.0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::FormatZoom
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DxuiZoomControl::FormatZoom (double zoom)
{
    return std::to_wstring (std::lround (zoom * 100)) + L"%";
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::Layout
//
//  From the right: the percentage, then, while the slider is visible, the
//  "+" button, the slider and the "-" button.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiZoomControl::Layout (const RECT & boundsPx, const DxuiDpiScaler & scaler)
{
    int  button  = scaler.ToPx (s_kButtonDip);
    int  slider  = scaler.ToPx (s_kSliderDip);
    int  percent = scaler.ToPx (s_kPercentDip);
    int  top     = boundsPx.top;
    int  x       = boundsPx.right - percent;



    SetBounds (boundsPx);
    m_scaler = scaler;

    m_percent->Layout ({ x, top, x + percent, boundsPx.bottom }, scaler);

    x -= m_isSliderShown ? button : 0;
    m_zoomIn->Layout  ({ x, top, x + button, boundsPx.bottom }, scaler);
    x -= m_isSliderShown ? slider : 0;
    m_slider->Layout  ({ x, top, x + slider, boundsPx.bottom }, scaler);
    x -= m_isSliderShown ? button : 0;
    m_zoomOut->Layout ({ x, top, x + button, boundsPx.bottom }, scaler);

    m_zoomIn->SetVisible  (m_isSliderShown);
    m_slider->SetVisible  (m_isSliderShown);
    m_zoomOut->SetVisible (m_isSliderShown);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiZoomControl::ChangeZoom
//
//  A zoom from the control's own buttons or slider: set on the control, then
//  passed to the callback.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiZoomControl::ChangeZoom (double zoom)
{
    SetZoom (zoom);

    if (m_onZoom)
    {
        m_onZoom (m_zoom);
    }
}
