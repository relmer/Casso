#include "Pch.h"

#include "Window/DxuiWindowSizer.h"
#include "Window/DxuiHwndSource.h"
#include "Core/DxuiDisplay.h"
#include "Core/DxuiWindowSizeMath.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizer::GetMinMax
//
//  Clamps the OS minimum track size to the minimum client size scaled to the
//  current DPI, and the maximum to what the content reports, never below the
//  minimum. The maximum is a client size, so the frame around a resizable
//  window's client is added to it. False when neither applies.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindowSizer::GetMinMax (MINMAXINFO & info) const
{
    DxuiHwndSource  * source = m_host.GetSizerSource();
    UINT              dpi    = source != nullptr ? source->GetScaler().GetDpi() : USER_DEFAULT_SCREEN_DPI;
    SIZE              maxPx  = {};
    bool              isSet  = false;



    if (m_minSizeDip.cx > 0 || m_minSizeDip.cy > 0)
    {
        info.ptMinTrackSize.x = MulDiv (m_minSizeDip.cx, (int) dpi, USER_DEFAULT_SCREEN_DPI);
        info.ptMinTrackSize.y = MulDiv (m_minSizeDip.cy, (int) dpi, USER_DEFAULT_SCREEN_DPI);
        isSet = true;
    }

    if (m_host.GetMaxTrackSizePx (maxPx))
    {
        maxPx = AddFrameSizePx (maxPx);

        info.ptMaxTrackSize.x = std::max (maxPx.cx, info.ptMinTrackSize.x);
        info.ptMaxTrackSize.y = std::max (maxPx.cy, info.ptMinTrackSize.y);
        isSet = true;
    }

    return isSet;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizer::ResizeToMaxSize
//
//  Fit sizes the window to the content's largest client size within its
//  monitor's work area, and places it there by its creation-time placement.
//  It is called before the window is first shown, once its content has been
//  laid out.
//
//  Grow grows the window toward that size within the work area, and leaves
//  it alone when the content fits.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiWindowSizer::ResizeToMaxSize (MaxSizeMode mode)
{
    HRESULT           hr      = S_OK;
    DxuiHwndSource  * source  = m_host.GetSizerSource();
    HWND              hwnd    = source != nullptr ? source->GetHwnd() : nullptr;
    HMONITOR          monitor = nullptr;
    MONITORINFO       info    = { sizeof (info) };
    RECT              rect    = {};
    RECT              fitted  = {};
    SIZE              maxPx   = {};
    SIZE              sizePx  = {};
    POINT             topLeft = {};
    bool              hasMax  = false;
    bool              isSame  = false;
    bool              grows   = false;
    BOOL              done    = FALSE;



    CBRA (hwnd != nullptr);

    hasMax = m_host.GetMaxClientSizePx (maxPx);
    BAIL_OUT_IF (!hasMax, S_OK);

    //  Windows computes a borderless window's resize borders only at a frame
    //  change, and a hidden window has had none since its creation: its
    //  client fills the window and the frame measures zero. A frame change is
    //  forced first.
    done = SetWindowPos (hwnd, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    CWRA (done);

    maxPx = AddFrameSizePx (maxPx);

    done = GetWindowRect (hwnd, &rect);
    CWRA (done);

    monitor = DxuiDisplay::Get().MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
    done    = DxuiDisplay::Get().GetMonitorInfoW (monitor, &info);
    CWRA (done);

    if (mode == MaxSizeMode::Grow)
    {
        grows = DxuiWindowSizeMath::TryGetGrownRect (rect, maxPx, info.rcWork, fitted);
        BAIL_OUT_IF (!grows, S_OK);
    }
    else
    {
        fitted = DxuiWindowSizeMath::FitRectToMaxSize (rect, maxPx, info.rcWork);

        // The fit runs while the window is still hidden, once its content can
        // be measured, so it is placed here at the size it really opens at:
        // the creation-time placement measured a size that was only a guess.
        sizePx = { fitted.right - fitted.left, fitted.bottom - fitted.top };

        if (source->TryGetPlacementForSize (sizePx, topLeft))
        {
            fitted = RECT { topLeft.x, topLeft.y, topLeft.x + sizePx.cx, topLeft.y + sizePx.cy };
        }
    }

    isSame = EqualRect (&fitted, &rect) != FALSE;
    BAIL_OUT_IF (isSame, S_OK);

    done = SetWindowPos (hwnd, nullptr, fitted.left, fitted.top,
                         fitted.right - fitted.left, fitted.bottom - fitted.top,
                         SWP_NOZORDER | SWP_NOACTIVATE);
    CWRA (done);

Error:
    return;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizer::AddFrameSizePx
//
//  A client size as the window size around it: a resizable window has
//  invisible resize borders outside its client, and a window size without
//  them makes the client that much smaller. Before the window has a client
//  to measure, the client size is returned as given.
//
////////////////////////////////////////////////////////////////////////////////

SIZE DxuiWindowSizer::AddFrameSizePx (const SIZE & clientPx) const
{
    DxuiHwndSource  * source = m_host.GetSizerSource();
    HWND              hwnd   = source != nullptr ? source->GetHwnd() : nullptr;
    RECT              window = {};
    RECT              client = {};
    SIZE              sizePx = clientPx;



    if (hwnd != nullptr && GetWindowRect (hwnd, &window) && GetClientRect (hwnd, &client) &&
        client.right > 0 && client.bottom > 0)
    {
        sizePx.cx += (window.right - window.left) - client.right;
        sizePx.cy += (window.bottom - window.top) - client.bottom;
    }

    return sizePx;
}
