#include "Pch.h"

#include "DxuiLightTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLightTheme::DxuiLightTheme
//
//  Base fill #F3F3F3, layer fill #FBFBFB, primary text #1A1A1A, secondary
//  text #5D5D5D, stroke #E5E5E5, and the light accent #005FB8. Selection is
//  the pale accent Explorer draws behind a selected row.
//
////////////////////////////////////////////////////////////////////////////////

DxuiLightTheme::DxuiLightTheme()
{
    titleBarTop              = 0xFFF3F3F3;
    titleBarBottom           = 0xFFF3F3F3;
    titleText                = DxuiWindowsThemeColors::kCaptionForegroundLight;
    bodyText                 = 0xFF1A1A1A;
    sysButtonIdle            = 0x00000000;
    sysButtonHover           = DxuiWindowsThemeColors::kSubtleFillColorSecondaryLight;
    sysButtonPressed         = DxuiWindowsThemeColors::kSubtleFillColorTertiaryLight;
    sysButtonCloseHover      = DxuiWindowsThemeColors::kCloseButtonColor;
    sysButtonCloseHoverGlyph = DxuiWindowsThemeColors::kCloseButtonGlyphHoverColor;
    sysButtonClosePressed    = DxuiWindowsThemeColors::kCloseButtonColor;
    navStrip                 = 0xFFF3F3F3;
    navHover                 = 0xFFCCE4F7;
    navItemText              = 0xFF1A1A1A;
    dropdownBg               = 0xFFF9F9F9;
    dropdownItemText         = 0xFF1A1A1A;
    dropdownAccel            = 0xFF5D5D5D;
    dropdownHover            = 0xFFEAEAEA;
    link                     = 0xFF005FB8;
    linkHover                = 0xFF003E92;
    panelBg                  = 0xFFFBFBFB;
    panelEdge                = 0xFFE5E5E5;
    buttonIdle               = 0xFFFBFBFB;
    buttonHover              = 0xFFF6F6F6;
    buttonPressed            = 0xFFF0F0F0;
    buttonBorder             = 0xFFD1D1D1;
    tooltipBg                = 0xFFF9F9F9;
    tooltipBorder            = 0xFFD1D1D1;
    tooltipText              = 0xFF1A1A1A;
    errorText                = 0xFFC42B1C;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiLightTheme::ApplySystemColors
//
//  On a light surface the accent is one step darker, Dark1 with Dark2 for
//  hover, as in Fluent.
//
//  Text color is unchanged. The visual style's list text color is the classic
//  style's value, #000000 for light where Windows 11 uses #1A1A1A, and body
//  text is also used for every menu and dialog.
//
////////////////////////////////////////////////////////////////////////////////

void DxuiLightTheme::ApplySystemColors (const DxuiWindowsThemeColors::SystemColors & colors)
{
    if (colors.hasSurfaces)
    {
        contentBg = colors.contentLight;
    }

    if (colors.hasAccent)
    {
        link      = colors.accentDark1;
        linkHover = colors.accentDark2;
    }
}
