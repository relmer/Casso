#include "Pch.h"

#include "Core/DxuiPaneMetrics.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneMetrics::GetLinePx
//
//  The width of a pane's outline, in whole pixels: one DIP rounded to the
//  nearest pixel, and never less than one.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPaneMetrics::GetLinePx (const DxuiDpiScaler & scaler)
{
    return (std::max) (1, (int) std::lround (scaler.ToPxf (1.0f)));
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneMetrics::GetCornerPx
//
//  The outer radius of a pane's outline: the inner corner radius plus the
//  outline's width, so the outline wraps the inner corner exactly.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPaneMetrics::GetCornerPx (const DxuiDpiScaler & scaler)
{
    return scaler.ToPx (kCornerDip) + GetLinePx (scaler);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneMetrics::GetTextInsetPx
//
//  From a pane's outer edge to the origin of a title, a tab label or the
//  first text inside it.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPaneMetrics::GetTextInsetPx (const DxuiDpiScaler & scaler)
{
    return scaler.ToPx (kTextInsetDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneMetrics::GetContentTextInsetPx
//
//  The same origin measured from a pane body's left, which lies one outline
//  width inside the pane's outer edge.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPaneMetrics::GetContentTextInsetPx (const DxuiDpiScaler & scaler)
{
    return GetTextInsetPx (scaler) - GetLinePx (scaler);
}





