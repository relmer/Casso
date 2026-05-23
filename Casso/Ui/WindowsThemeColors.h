#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  WindowsThemeColors
//
//  Runtime accessor for Windows-system theme colors used by the custom
//  chrome (caption buttons, title-bar accents). Reads the current
//  Windows light/dark mode from the per-user theme registry value and
//  surfaces the authoritative Fluent token values for the active theme.
//
//  Sources for the token values used here:
//   * microsoft/microsoft-ui-xaml -- Common_themeresources_any.xaml
//     defines `SubtleFillColorSecondary` (hover) and
//     `SubtleFillColorTertiary` (pressed) for both light and dark.
//   * microsoft/terminal -- MinMaxCloseControl.xaml wires the WinUI
//     tokens to the caption buttons and pins `CloseButtonColor` to
//     #C42B1C for both hover and pressed in both light and dark.
//
//  Refresh model: `Refresh()` re-reads the registry. Cheap; safe to
//  call in response to `WM_SETTINGCHANGE` with `lParam` pointing at
//  the `"ImmersiveColorSet"` string.
//
////////////////////////////////////////////////////////////////////////////////

class WindowsThemeColors
{
public:
    static WindowsThemeColors & Instance ();

    void  Refresh ();
    bool  IsDarkMode () const { return m_darkMode; }

    uint32_t  CaptionButtonHoverArgb        () const;
    uint32_t  CaptionButtonPressedArgb      () const;
    uint32_t  CaptionButtonForegroundArgb   () const;
    uint32_t  CloseButtonHoverArgb           () const;
    uint32_t  CloseButtonPressedArgb         () const;
    uint32_t  CloseButtonGlyphHoverArgb      () const;
    uint32_t  CloseButtonGlyphPressedArgb    () const;

private:
    WindowsThemeColors ();

    bool  m_darkMode = true;
};
