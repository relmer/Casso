#include "Pch.h"

#include "Core/DxuiWindowFrame.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowFrame::GetVisibleRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiWindowFrame::GetVisibleRect (HWND hwnd)
{
    RECT     rect    = {};
    RECT     visible = {};
    HRESULT  hr      = E_FAIL;



    if (hwnd == nullptr || !GetWindowRect (hwnd, &rect))
    {
        return rect;
    }

    hr = DwmGetWindowAttribute (hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &visible, sizeof (visible));

    //  An empty answer is no answer: DWM reports zeros for a window that has
    //  not been composited yet, and a zero rect would move the window to the
    //  top-left corner of the desktop.
    if (FAILED (hr) || (visible.right - visible.left) <= 0 || (visible.bottom - visible.top) <= 0)
    {
        return rect;
    }

    return visible;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowFrame::GetBorder
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiWindowFrame::GetBorder (HWND hwnd)
{
    RECT  window  = {};
    RECT  visible = GetVisibleRect (hwnd);
    RECT  border  = {};



    if (hwnd == nullptr || !GetWindowRect (hwnd, &window))
    {
        return border;
    }

    border.left   = (std::max) (0L, visible.left   - window.left);
    border.top    = (std::max) (0L, visible.top    - window.top);
    border.right  = (std::max) (0L, window.right   - visible.right);
    border.bottom = (std::max) (0L, window.bottom  - visible.bottom);

    return border;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowFrame::ToWindowRect
//
////////////////////////////////////////////////////////////////////////////////

RECT DxuiWindowFrame::ToWindowRect (HWND hwnd, const RECT & visible)
{
    RECT  border = GetBorder (hwnd);
    RECT  window = visible;



    window.left   -= border.left;
    window.top    -= border.top;
    window.right  += border.right;
    window.bottom += border.bottom;

    return window;
}
