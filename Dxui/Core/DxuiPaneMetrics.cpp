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
//  The outer radius of a pane's outline: the corner's radius along the
//  middle of the outline plus the half of the outline outside it, rounded
//  up. That is the rule WPF's Border draws by, and what Visual Studio
//  shows: 5, 6 and 7 px at 100%, 125% and 150%, where the outline is 1, 1
//  and 2 px wide.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPaneMetrics::GetCornerPx (const DxuiDpiScaler & scaler)
{
    return scaler.ToPx (kCornerDip) + (GetLinePx (scaler) + 1) / 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneMetrics::GetTextInsetPx
//
//  From a pane's outer edge to the origin of its title, and of every other
//  text in the pane, which starts where the title does: a tab label, a
//  toolbar's first content, a list's first column. Visual Studio places a
//  title this far in: the outline and 8 DIP more, 9, 11 and 14 px at 100%,
//  125% and 150%, so a title's first ink lands 12 px in at 125%.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPaneMetrics::GetTextInsetPx (const DxuiDpiScaler & scaler)
{
    return GetLinePx (scaler) + scaler.ToPx (kTextInsetDip);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneMetrics::GetContentTextInsetPx
//
//  The same origin measured from a pane body's left, which lies one outline
//  width inside the pane's outer edge: 8 DIP in whole pixels.
//
////////////////////////////////////////////////////////////////////////////////

int DxuiPaneMetrics::GetContentTextInsetPx (const DxuiDpiScaler & scaler)
{
    return GetTextInsetPx (scaler) - GetLinePx (scaler);
}





