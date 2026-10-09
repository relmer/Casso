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
//  The strip under the tabs takes the layer fill, a step above the caption,
//  as the dark theme's does, so the selected tab that joins it shows, and the
//  address box set into it is white, a step above again. Unlike the dark
//  theme's, these have not yet been measured against Explorer.
//
//  A tooltip is File Explorer's: #F9F9F9 bordered in #D1D1D1, with the body
//  text color.
//
//  The drop targets a dragged pane shows are measured from Visual Studio
//  2026's light drop targets at 125% on 2026-10-08: the cross's border
//  #CCCEDB over a translucent #E8E8ED, and the button under the pointer
//  #F5F5F5 with no border, its picture #006CBE and its arrow #1E1E1E. Every
//  other button shows those at 70%, the #F3F3F4, #4893CE and #5D5D5E the
//  captures read. The shade over where the pane would go is #0078D4 at
//  alpha 0x1E.
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
    navStrip                 = 0xFFFBFBFB;
    navHover                 = 0xFFCCE4F7;
    navItemText              = 0xFF1A1A1A;
    dropdownBg               = 0xFFF9F9F9;
    dropdownItemText         = 0xFF1A1A1A;
    dropdownAccel            = 0xFF5D5D5D;
    dropdownHover            = 0xFFEAEAEA;
    link                     = 0xFF005FB8;
    linkHover                = 0xFF003E92;

    //  The outline on a selected row while its list or tree holds focus. The
    //  dark theme has carried one since it shipped; without it here, focus is
    //  invisible in the light theme for every widget that draws one.
    contentSelectionEdge     = 0xFF5D5D5D;
    panelBg                  = 0xFFFBFBFB;
    panelEdge                = 0xFFE5E5E5;
    controlBg                = 0xFFFFFFFF;
    buttonIdle               = 0xFFFBFBFB;
    buttonHover              = 0xFFF6F6F6;
    buttonPressed            = 0xFFF0F0F0;
    buttonBorder             = 0xFFD1D1D1;
    tooltipBg                = 0xFFF9F9F9;
    tooltipBorder            = 0xFFD1D1D1;
    tooltipText              = 0xFF1A1A1A;
    errorText                = 0xFFC42B1C;
    resultText               = 0xFF00727D;
    dockGuideBorder          = 0xFFCCCEDB;
    dockGuideFill            = 0x99E8E8ED;

    //  Transparent, not zero, which would take the derived color: Visual
    //  Studio's light theme draws no border around a drop target's button.
    dockGuideButtonBorder    = 0x00F5F5F5;
    dockGuideButtonFill      = 0xFFF5F5F5;
    dockGuideGlyph           = 0xFF006CBE;
    dockGuideArrow           = 0xFF1E1E1E;
    dockPreview              = 0x1E0078D4;
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
