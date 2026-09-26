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
//  The chrome is measured from Explorer, as the dark theme's is, with its
//  window inactive at 120 dpi on 2026-09-23: the caption is #E8E8E8, the strip
//  under the tabs #F8F8F8, and the address box set into it #FDFDFD. The list,
//  the command bar and the strip along the bottom are all #FFFFFF, with a
//  #D6D6D6 line above and below the command bar. A row under the pointer is
//  #E5F3FF.
//
//  Measured again at 120 and 192 dpi on 2026-09-25: a selected row is #CCE8FF
//  outlined in #000000 while its pane has focus, and #D9D9D9 outlined in
//  #949494 while it does not; the lines between the list's column titles are
//  #E5E5E5; and a #DADADA line runs under the tab strip, broken under the
//  selected tab.
//
////////////////////////////////////////////////////////////////////////////////

DxuiLightTheme::DxuiLightTheme()
{
    titleBarTop              = 0xFFE8E8E8;
    titleBarBottom           = 0xFFE8E8E8;
    titleText                = DxuiWindowsThemeColors::kCaptionForegroundLight;
    bodyText                 = 0xFF1A1A1A;
    sysButtonIdle            = 0x00000000;
    sysButtonHover           = DxuiWindowsThemeColors::kSubtleFillColorSecondaryLight;
    sysButtonPressed         = DxuiWindowsThemeColors::kSubtleFillColorTertiaryLight;
    sysButtonCloseHover      = DxuiWindowsThemeColors::kCloseButtonColor;
    sysButtonCloseHoverGlyph = DxuiWindowsThemeColors::kCloseButtonGlyphHoverColor;
    sysButtonClosePressed    = DxuiWindowsThemeColors::kCloseButtonColor;
    navStrip                 = 0xFFF8F8F8;
    navHover                 = 0xFFCCE4F7;
    navItemText              = 0xFF1A1A1A;
    dropdownBg               = 0xFFF9F9F9;
    dropdownItemText         = 0xFF1A1A1A;
    dropdownAccel            = 0xFF5D5D5D;
    dropdownHover            = 0xFFEAEAEA;
    link                     = 0xFF005FB8;
    linkHover                = 0xFF003E92;

    contentSelection             = 0xFFCCE8FF;
    contentSelectionEdge         = 0xFF000000;
    contentSelectionInactive     = 0xFFD9D9D9;
    contentSelectionInactiveEdge = 0xFF949494;
    contentHeaderDivider         = 0xFFE5E5E5;
    tabStripEdge                 = 0xFFDADADA;
    panelBg                  = 0xFFFBFBFB;
    panelEdge                = 0xFFD6D6D6;
    contentBg                = 0xFFFFFFFF;
    statusBg                 = 0xFFFFFFFF;
    contentHover             = 0xFFE5F3FF;
    controlBg                = 0xFFFDFDFD;
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
