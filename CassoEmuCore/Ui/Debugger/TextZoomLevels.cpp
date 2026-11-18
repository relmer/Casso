#include "Pch.h"

#include "Ui/Debugger/TextZoomLevels.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TextZoomLevels::GetSliderPosition
//
//  The size's logarithm across the slider, from kMinZoom at the left to
//  kMaxZoom at the right, as the heat map's cell size is.
//
////////////////////////////////////////////////////////////////////////////////

float TextZoomLevels::GetSliderPosition (float zoom)
{
    double  span = std::log ((double) kMaxZoom / (double) kMinZoom);



    return (float) std::clamp (std::log ((double) zoom / (double) kMinZoom) / span, 0.0, 1.0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextZoomLevels::GetZoomAtSlider
//
//  The size at a slider position, to the nearest kZoomStep.
//
////////////////////////////////////////////////////////////////////////////////

float TextZoomLevels::GetZoomAtSlider (float position)
{
    double  span = std::log ((double) kMaxZoom / (double) kMinZoom);
    double  zoom = (double) kMinZoom * std::exp (std::clamp ((double) position, 0.0, 1.0) * span);



    zoom = std::round (zoom / (double) kZoomStep) * (double) kZoomStep;
    return (float) std::clamp (zoom, (double) kMinZoom, (double) kMaxZoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextZoomLevels::Step
//
//  Each size is a whole number of ten-percentage-point steps, so a step past
//  either end is lost rather than stored as a clamped fraction, and the same
//  number of steps back always returns to the size they left.
//
////////////////////////////////////////////////////////////////////////////////

float TextZoomLevels::Step (float zoom, int steps)
{
    int  smallest = (int) std::lround (kMinZoom / kZoomStep);
    int  largest  = (int) std::lround (kMaxZoom / kZoomStep);
    int  step     = (int) std::lround (zoom / kZoomStep);



    step = std::clamp (step + steps, smallest, largest);
    return (float) step * kZoomStep;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextZoomLevels::SetWindowZoom
//
////////////////////////////////////////////////////////////////////////////////

void TextZoomLevels::SetWindowZoom (float zoom)
{
    m_windowZoom = std::clamp (zoom, kMinZoom, kMaxZoom);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextZoomLevels::GetPaneZoom
//
//  A pane's text size over the window's, 1 for a pane never zoomed.
//
////////////////////////////////////////////////////////////////////////////////

float TextZoomLevels::GetPaneZoom (const std::wstring & pane) const
{
    auto  found = m_paneZooms.find (pane);



    return (found != m_paneZooms.end()) ? found->second : 1.0f;
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextZoomLevels::StepPaneZoom
//
//  One pane's text alone, in the same steps as the window's. Zero steps
//  returns the pane to the window's size.
//
////////////////////////////////////////////////////////////////////////////////

void TextZoomLevels::StepPaneZoom (const std::wstring & pane, int steps)
{
    if (steps == 0)
    {
        m_paneZooms.erase (pane);
        return;
    }

    m_paneZooms[pane] = Step (GetPaneZoom (pane), steps);
}





////////////////////////////////////////////////////////////////////////////////
//
//  TextZoomLevels::GetEffectiveZoom
//
//  A pane's text at the window's size times the pane's, within the limits.
//
////////////////////////////////////////////////////////////////////////////////

float TextZoomLevels::GetEffectiveZoom (const std::wstring & pane) const
{
    return std::clamp (m_windowZoom * GetPaneZoom (pane), kMinZoom, kMaxZoom);
}
