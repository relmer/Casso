#include "Pch.h"

#include "DxuiDarkTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDarkTheme::DxuiDarkTheme
//
//  Base fill #202020, layer fill #272727, primary text #FFFFFF, secondary
//  text #C5C5C5, stroke #3A3A3A, and the dark accent #60CDFF. Selection is
//  the muted accent Explorer draws behind a selected row.
//
//  The content and status fills are measured from Explorer rather than taken
//  from a token sheet: its list body, its navigation pane and its command bar
//  are all #191919, and the strip along the bottom is #1C1C1C. Captured at
//  120 dpi on 2026-09-12 and compared pixel for pixel.
//
////////////////////////////////////////////////////////////////////////////////

DxuiDarkTheme::DxuiDarkTheme()
{
    titleBarTop              = 0xFF202020;
    titleBarBottom           = 0xFF202020;
    titleText                = DxuiWindowsThemeColors::kCaptionForegroundDark;
    bodyText                 = 0xFFFFFFFF;
    sysButtonIdle            = 0x00000000;
    sysButtonHover           = DxuiWindowsThemeColors::kSubtleFillColorSecondaryDark;
    sysButtonPressed         = DxuiWindowsThemeColors::kSubtleFillColorTertiaryDark;
    sysButtonCloseHover      = DxuiWindowsThemeColors::kCloseButtonColor;
    sysButtonCloseHoverGlyph = DxuiWindowsThemeColors::kCloseButtonGlyphHoverColor;
    sysButtonClosePressed    = DxuiWindowsThemeColors::kCloseButtonColor;
    navStrip                 = 0xFF202020;
    navHover                 = 0xFF194A6B;
    navItemText              = 0xFFFFFFFF;
    dropdownBg               = 0xFF2C2C2C;
    dropdownItemText         = 0xFFFFFFFF;
    dropdownAccel            = 0xFFC5C5C5;
    dropdownHover            = 0xFF383838;
    link                     = 0xFF60CDFF;
    linkHover                = 0xFF99EBFF;
    panelBg                  = 0xFF272727;
    contentBg                = 0xFF191919;
    statusBg                 = 0xFF1C1C1C;
    headingText              = 0xFFDEDEDE;
    contentEdge              = 0xFF1D1D1D;
    splitterHighlight        = 0xFF2B2B2B;
    contentHover             = 0xFF232323;
    contentSelection         = 0xFF2D2D2D;
    panelEdge                = 0xFF3A3A3A;
    buttonIdle               = 0xFF2D2D2D;
    buttonHover              = 0xFF323232;
    buttonPressed            = 0xFF272727;
    buttonBorder             = 0xFF4A4A4A;
    tooltipBg                = 0xFF2C2C2C;
    tooltipBorder            = 0xFF4A4A4A;
    tooltipText              = 0xFFFFFFFF;
    errorText                = 0xFFFF99A4;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiDarkTheme::ApplySystemColors
//
//  On a dark surface the accent is one step lighter, Light2 with Light3 for
//  hover, as in Fluent.
//
//  Text color is unchanged. The visual style's list text color is the classic
//  style's value, #000000 for light where Windows 11 uses #1A1A1A, and body
//  text is also used for every menu and dialog.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiDarkTheme::ApplySystemColors (const DxuiWindowsThemeColors::SystemColors & colors)
{
    if (colors.hasSurfaces)
    {
        contentBg = colors.contentDark;
    }

    if (colors.hasAccent)
    {
        link      = colors.accentLight2;
        linkHover = colors.accentLight3;
    }
}
