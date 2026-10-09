#pragma once

#include "Pch.h"
#include "Core/DxuiDpiScaler.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiPaneMetrics
//
//  The measures every docked or floating pane shares, so a pane's frame and
//  the text inside it agree to the pixel. Visual Studio at 125% is the
//  reference.
//
////////////////////////////////////////////////////////////////////////////////

class DxuiPaneMetrics
{
public:
    //  The radius of a pane's corner along the middle of its outline.
    static constexpr int  kCornerDip    = 4;

    //  From the inside of a pane's outline to the origin of its text.
    static constexpr int  kTextInsetDip = 8;

    //  The outer radius Windows 11 rounds a window's corners by, which a
    //  floating pane's outline follows at the corners it shares with its
    //  window: 8, 10 and 12 px at 100%, 125% and 150%.
    static constexpr int  kWindowCornerDip = 8;

    static int  GetLinePx             (const DxuiDpiScaler & scaler);
    static int  GetCornerPx           (const DxuiDpiScaler & scaler);
    static int  GetTextInsetPx        (const DxuiDpiScaler & scaler);
    static int  GetContentTextInsetPx (const DxuiDpiScaler & scaler);
};
