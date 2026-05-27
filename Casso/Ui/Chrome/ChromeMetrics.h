#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  ChromeMetrics
//
//  Pure constants for the emulator framebuffer and base DPI.
//  Chrome inset math now lives in `ChromeLayout`; this header only
//  carries dimensions that are intrinsic to the Apple II framebuffer
//  and the chrome's authoring DPI.
//
////////////////////////////////////////////////////////////////////////////////

namespace ChromeMetrics
{
    constexpr int  kFramebufferWidthPx   = 560;
    constexpr int  kFramebufferHeightPx  = 384;
    constexpr int  kBaseDpi              = 96;
}
