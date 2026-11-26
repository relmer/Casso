#include "Pch.h"

#include "Window/DxuiWindowSizer.h"
#include "Window/DxuiHwndSource.h"
#include "Core/DxuiWindowSizeMath.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiWindowSizer::GetMinMax
//
//  Clamps the OS minimum track size to the minimum client size scaled to the
//  current DPI, and the maximum to what the content reports, never below the
//  minimum. Borderless, so client size and window size coincide. False when
//  neither applies.
//
////////////////////////////////////////////////////////////////////////////////

bool DxuiWindowSizer::GetMinMax (MINMAXINFO & info) const
{
    DxuiHwndSource  * source = m_host.GetSizerSource();
    UINT              dpi    = source != nullptr ? source->GetScaler().GetDpi() : USER_DEFAULT_SCREEN_DPI;
    SIZE              maxPx  = {};
    bool              hasMax = m_host.GetMaxTrackSizePx (maxPx);



    return DxuiWindowSizeMath::TryApplyTrackLimits (m_minSizeDip, dpi, hasMax, maxPx, info);
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

    done = GetWindowRect (hwnd, &rect);
    CWRA (done);

    monitor = MonitorFromWindow (hwnd, MONITOR_DEFAULTTONEAREST);
    done    = GetMonitorInfoW (monitor, &info);
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
