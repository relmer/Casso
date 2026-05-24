#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeMetrics
//
//  Single source of truth for the chrome's vertical insets.
//  EmulatorShell uses these to size the window and tell D3DRenderer
//  where the content area starts/ends; WindowCommandManager uses them
//  for the Ctrl+0 "reset window size" path; chrome painters use them
//  to lay out the title bar, nav strip, and command bar. Drift between
//  these copies was the source of the pillarbox on Ctrl+0 where the
//  WCM path forgot to reserve space for the command bar entirely.
//
////////////////////////////////////////////////////////////////////////////////

namespace ChromeMetrics
{
    constexpr int  kFramebufferWidthPx   = 560;
    constexpr int  kFramebufferHeightPx  = 384;
    constexpr int  kBaseDpi              = 96;
    constexpr int  kTitleBarHeightDp     = 32;
    constexpr int  kNavStripHeightDp     = 32;
    constexpr int  kCommandBarHeightDp   = 192;


    inline int  ScaleForDpi (int dp, UINT dpi)
    {
        return MulDiv (dp, static_cast<int> (dpi), kBaseDpi);
    }


    inline int  TitleBarHeightPx (UINT dpi)
    {
        return ScaleForDpi (kTitleBarHeightDp, dpi);
    }


    inline int  NavStripHeightPx (UINT dpi)
    {
        return ScaleForDpi (kNavStripHeightDp, dpi);
    }


    inline int  CommandBarHeightPx (UINT dpi)
    {
        return ScaleForDpi (kCommandBarHeightDp, dpi);
    }


    inline int  ChromeTopInsetPx (UINT dpi)
    {
        return TitleBarHeightPx (dpi) + NavStripHeightPx (dpi);
    }


    inline int  ChromeBottomInsetPx (UINT dpi)
    {
        return CommandBarHeightPx (dpi);
    }
}
