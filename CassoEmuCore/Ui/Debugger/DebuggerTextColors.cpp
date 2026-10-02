#include "Pch.h"

#include "Ui/Debugger/DebuggerTextColors.h"
#include "Theme/DxuiColor.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::Make
//
//  The keyword blue for a mnemonic, the control purple for a directive, the
//  type teal for a symbol, and Visual Studio's number, string and comment
//  colors. A listing's address takes the theme's text color and its bytes the
//  muted one.
//
////////////////////////////////////////////////////////////////////////////////

DebuggerTextColors::Set DebuggerTextColors::Make (
    uint32_t background,
    uint32_t foreground,
    uint32_t muted,
    uint32_t resultText)
{
    Set       set;
    bool      dark    = IsDark (background);
    uint32_t  against = dark ? background : GetRowShade (background);
    uint32_t  text    = dark ? foreground : DxuiColor::Mix (foreground, background, s_kLightTextSoftening);



    set.syntax.mnemonic  = GetReadable (dark ? 0xFF569CD6 : 0xFF0000FF, against);
    set.syntax.directive = GetReadable (dark ? 0xFFC586C0 : 0xFFAF00DB, against);
    set.syntax.symbol    = GetReadable (dark ? 0xFF4EC9B0 : 0xFF2B91AF, against);
    set.syntax.number    = GetReadable (dark ? 0xFFB5CEA8 : 0xFF098658, against);
    set.syntax.string    = GetReadable (dark ? 0xFFD69D85 : 0xFFA31515, against);
    set.syntax.address   = GetReadable (text,       against);
    set.syntax.bytes     = GetReadable (muted,      against);
    set.annotation       = GetReadable (dark ? 0xFF57A64A : 0xFF008000, against);
    set.changed          = GetReadable (dark ? 0xFFFF6B68 : 0xFFD00000, against);
    set.result           = GetReadable ((resultText != 0) ? resultText : (dark ? 0xFF4EC9E0 : 0xFF00727D), against);
    set.muted            = set.syntax.bytes;
    set.syntax.comment   = set.annotation;

    return set;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::GetRowShade
//
//  On a light page the selected row and the PC's row are tinted darker than
//  the page, so text must read on the tint as well. The shade stands in for
//  the darkest of those tints.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerTextColors::GetRowShade (uint32_t background)
{
    return DxuiColor::Mix (background | 0xFF000000u, 0xFF000000u, s_kLightRowShade);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::IsDark
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerTextColors::IsDark (uint32_t background)
{
    uint32_t  luma = ((background >> 16) & 0xFF) * 299 + ((background >> 8) & 0xFF) * 587 + (background & 0xFF) * 114;



    return luma < 128 * 1000;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerTextColors::GetReadable
//
//  Mixes in black or white a twentieth at a time, so a color that already
//  reads is left exactly as it was and one that does not keeps as much of
//  its hue as it can.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t DebuggerTextColors::GetReadable (uint32_t argb, uint32_t background)
{
    constexpr int  s_kSteps = 20;
    uint32_t       opaque   = argb | 0xFF000000u;
    uint32_t       toward   = IsDark (background) ? 0xFFFFFFFFu : 0xFF000000u;
    uint32_t       result   = opaque;
    int            step     = 0;



    for (step = 1; step <= s_kSteps && DxuiColor::ComputeContrastRatio (result, background) < s_kMinTextContrast; step++)
    {
        result = DxuiColor::Mix (opaque, toward, (float) step / (float) s_kSteps);
    }

    return result;
}
