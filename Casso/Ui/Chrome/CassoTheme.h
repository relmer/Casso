#pragma once

#include "Pch.h"
#include "Theme/DxuiTheme.h"





struct ChromeVisualState
{
    UINT     dpi        = 96;
    int64_t  nowMs      = 0;
    int      frameIndex = 0;
};


////////////////////////////////////////////////////////////////////////////////
//
//  CassoTheme
//
//  Casso's theme: the generic Dxui-rendered tokens come from DxuiTheme;
//  CassoTheme adds the app-specific bits Dxui has no concept of -- the
//  skeuomorphic Disk II drive widgets and their LEDs -- plus the preset
//  palettes (Skeuomorphic / DarkModern / RetroTerminal). WCAG colour math
//  lives in Dxui/Theme/DxuiColor.h; widgets derive accessible tints there.
//
////////////////////////////////////////////////////////////////////////////////

struct CassoTheme : public DxuiTheme
{
    // Whether the drive widgets use the compact paint path (small flat
    // card with label + LED). False = full skeuomorphic Apple Disk II
    // widgets. The drive-bar thickness contracts with this flag via
    // EmulatorShell's theme listener so the emulator pixel grid is
    // preserved across theme swaps.
    bool      compactDrives  = false;

    uint32_t  driveBody  = 0;
    uint32_t  driveBezel = 0;
    uint32_t  driveLabel = 0;
    uint32_t  ledIdle    = 0;
    uint32_t  ledPresent = 0;
    uint32_t  ledActive  = 0;
    uint32_t  ledHalo    = 0;

    static CassoTheme Skeuomorphic()
    {
        CassoTheme  theme = {};



        theme.titleBarTop         = 0xFF102A44;
        theme.titleBarBottom      = 0xFF071827;
        theme.titleText           = 0xFFE8EEF4;
        // Caption-button overlays match the authoritative Fluent
        // dark-mode spec from microsoft/microsoft-ui-xaml
        // (Common_themeresources_any.xaml -> SubtleFillColor*) and
        // microsoft/terminal (MinMaxCloseControl.xaml -> CloseButtonColor):
        //   * idle is fully transparent so the title-bar gradient
        //     reads through
        //   * hover uses SubtleFillColorSecondary (0x0FFFFFFF) and
        //     pressed uses SubtleFillColorTertiary (0x0AFFFFFF) for
        //     min and max
        //   * close hover and close pressed both use the documented
        //     Windows red (0xFFC42B1C) -- Microsoft's WinUI close
        //     button does NOT darken on press, the same red is
        //     reused; the glyph flips to opaque white in both states
        //     so the X reads against the red fill
        theme.sysButtonIdle          = 0x00000000;
        theme.sysButtonHover         = 0x0FFFFFFF;
        theme.sysButtonPressed       = 0x0AFFFFFF;
        theme.sysButtonCloseHover       = 0xFFC42B1C;
        theme.sysButtonCloseHoverGlyph  = 0xFFFFFFFF;
        theme.sysButtonClosePressed     = 0xFFC42B1C;
        theme.navStrip            = 0xFF182536;
        theme.navHover            = 0xFF2D4058;
        theme.navItemText         = 0xFFE8EEF4;
        theme.dropdownBg          = 0xFF202A35;
        theme.dropdownItemText    = 0xFFF3EAD7;
        theme.dropdownAccel       = 0xFFB8C0CA;
        theme.dropdownHover       = 0xFF34475F;
        theme.driveBody           = 0xFF141414;
        theme.driveBezel          = 0xFF050505;
        theme.driveLabel          = 0xFFE6E2D8;
        theme.ledIdle             = 0xFF1A0606;
        theme.ledPresent          = 0xFF1A0606;
        theme.ledActive           = 0xFFFF2818;
        theme.ledHalo             = 0x60FF2818;
        theme.link                = 0xFF6FB8FF;
        theme.linkHover           = 0xFFB7DFFF;
        theme.panelBg             = 0xFF1A2230;
        theme.panelEdge           = 0xFF334050;
        theme.buttonIdle          = 0xFF2D3F58;
        theme.buttonHover         = 0xFF3D547A;
        theme.buttonPressed       = 0xFF1F2C40;
        theme.buttonBorder        = 0xFF4A5F80;
        theme.tooltipBg           = 0xFF24304A;
        theme.tooltipBorder       = 0xFF4A5F80;
        theme.tooltipText         = 0xFFE8EEF4;
        theme.errorText           = 0xFFFF6B6B;
        return theme;
    }


    static CassoTheme DarkModern()
    {
        CassoTheme  theme = {};



        // Win11-style flat dark: graphite gradient, near-pure-white
        // labels, cool blue accent for hover/highlight, blue LEDs.
        theme.compactDrives                 = true;
        theme.titleBarTop               = 0xFF202225;
        theme.titleBarBottom            = 0xFF17181B;
        theme.titleText                 = 0xFFF0F0F0;
        theme.sysButtonIdle             = 0x00000000;
        theme.sysButtonHover            = 0x0FFFFFFF;
        theme.sysButtonPressed          = 0x0AFFFFFF;
        theme.sysButtonCloseHover       = 0xFFC42B1C;
        theme.sysButtonCloseHoverGlyph  = 0xFFFFFFFF;
        theme.sysButtonClosePressed     = 0xFFC42B1C;
        theme.navStrip                  = 0xFF2A2D32;
        theme.navHover                  = 0xFF3D6FB5;
        theme.navItemText               = 0xFFF0F0F0;
        theme.dropdownBg                = 0xFF26282C;
        theme.dropdownItemText          = 0xFFF0F0F0;
        theme.dropdownAccel             = 0xFFB8C0CA;
        theme.dropdownHover             = 0xFF3D6FB5;
        theme.driveBody                 = 0xFF1A1C1F;
        theme.driveBezel                = 0xFF050505;
        theme.driveLabel                = 0xFFE6E2D8;
        theme.ledIdle                   = 0xFF06121A;
        theme.ledPresent                = 0xFF06121A;
        theme.ledActive                 = 0xFF3DA1FF;
        theme.ledHalo                   = 0x603DA1FF;
        theme.link                      = 0xFF6FB8FF;
        theme.linkHover                 = 0xFFA8D2FF;
        theme.panelBg                   = 0xFF1E2024;
        theme.panelEdge                 = 0xFF3A3D42;
        theme.buttonIdle                = 0xFF323539;
        theme.buttonHover               = 0xFF45494F;
        theme.buttonPressed             = 0xFF23252A;
        theme.buttonBorder              = 0xFF55595F;
        theme.tooltipBg                 = 0xFF2E3035;
        theme.tooltipBorder             = 0xFF55595F;
        theme.tooltipText               = 0xFFF0F0F0;
        theme.errorText                 = 0xFFFF6B6B;
        return theme;
    }


    static CassoTheme RetroTerminal()
    {
        CassoTheme  theme = {};



        // P1 phosphor on near-black: deep green-black panels with
        // bright phosphor-green labels and LEDs to evoke a CRT cabinet.
        theme.compactDrives                 = true;
        theme.titleBarTop               = 0xFF16381E;
        theme.titleBarBottom            = 0xFF0A1F0E;
        theme.titleText                 = 0xFFB7FCB9;
        theme.sysButtonIdle             = 0x00000000;
        theme.sysButtonHover            = 0x14B7FCB9;
        theme.sysButtonPressed          = 0x0AB7FCB9;
        theme.sysButtonCloseHover       = 0xFFC42B1C;
        theme.sysButtonCloseHoverGlyph  = 0xFFFFFFFF;
        theme.sysButtonClosePressed     = 0xFFC42B1C;
        theme.navStrip                  = 0xFF102714;
        theme.navHover                  = 0xFF1F4A28;
        theme.navItemText               = 0xFFB7FCB9;
        theme.dropdownBg                = 0xFF12301A;
        theme.dropdownItemText          = 0xFFB7FCB9;
        theme.dropdownAccel             = 0xFF6DA875;
        theme.dropdownHover             = 0xFF1F4A28;
        theme.driveBody                 = 0xFF0F1A12;
        theme.driveBezel                = 0xFF040A05;
        theme.driveLabel                = 0xFFB7FCB9;
        theme.ledIdle                   = 0xFF071907;
        theme.ledPresent                = 0xFF071907;
        theme.ledActive                 = 0xFF2BFF6A;
        theme.ledHalo                   = 0x602BFF6A;
        theme.link                      = 0xFF8AFF8A;
        theme.linkHover                 = 0xFFB7FCB9;
        theme.panelBg                   = 0xFF0E2612;
        theme.panelEdge                 = 0xFF2A5C30;
        theme.buttonIdle                = 0xFF1A3F22;
        theme.buttonHover               = 0xFF286036;
        theme.buttonPressed             = 0xFF0F2814;
        theme.buttonBorder              = 0xFF3A7548;
        theme.tooltipBg                 = 0xFF154A22;
        theme.tooltipBorder             = 0xFF3A7548;
        theme.tooltipText               = 0xFFB7FCB9;
        theme.errorText                 = 0xFFFF6B6B;
        return theme;
    }


    static CassoTheme ForName (const std::string & name)
    {
        if (name == "DarkModern")
        {
            return DarkModern();
        }
        if (name == "RetroTerminal")
        {
            return RetroTerminal();
        }
        return Skeuomorphic();
    }
};