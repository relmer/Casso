#pragma once

#include "Pch.h"





struct ChromeVisualState
{
    UINT     dpi        = 96;
    int64_t  nowMs      = 0;
    int      frameIndex = 0;
};


struct ChromeTheme
{
    uint32_t  titleBarTopArgb        = 0;
    uint32_t  titleBarBottomArgb     = 0;
    uint32_t  titleTextArgb          = 0;
    uint32_t  sysButtonIdleArgb      = 0;
    uint32_t  sysButtonHoverArgb     = 0;
    uint32_t  sysButtonPressedArgb   = 0;
    uint32_t  sysButtonCloseHoverArgb   = 0;
    uint32_t  sysButtonCloseHoverGlyphArgb = 0;
    uint32_t  sysButtonClosePressedArgb = 0;
    uint32_t  navStripArgb           = 0;
    uint32_t  navHoverArgb           = 0;
    uint32_t  navItemTextArgb        = 0;
    uint32_t  dropdownBgArgb         = 0;
    uint32_t  dropdownItemTextArgb   = 0;
    uint32_t  dropdownAccelArgb      = 0;
    uint32_t  dropdownHoverArgb      = 0;
    uint32_t  driveBodyArgb          = 0;
    uint32_t  driveBezelArgb         = 0;
    uint32_t  driveLabelArgb         = 0;
    uint32_t  ledIdleArgb            = 0;
    uint32_t  ledPresentArgb         = 0;
    uint32_t  ledActiveArgb          = 0;
    uint32_t  ledHaloArgb            = 0;

    static ChromeTheme Skeuomorphic()
    {
        ChromeTheme  theme = {};



        theme.titleBarTopArgb         = 0xFF102A44;
        theme.titleBarBottomArgb      = 0xFF071827;
        theme.titleTextArgb           = 0xFFE8EEF4;
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
        theme.sysButtonIdleArgb          = 0x00000000;
        theme.sysButtonHoverArgb         = 0x0FFFFFFF;
        theme.sysButtonPressedArgb       = 0x0AFFFFFF;
        theme.sysButtonCloseHoverArgb       = 0xFFC42B1C;
        theme.sysButtonCloseHoverGlyphArgb  = 0xFFFFFFFF;
        theme.sysButtonClosePressedArgb     = 0xFFC42B1C;
        theme.navStripArgb            = 0xFF182536;
        theme.navHoverArgb            = 0xFF2D4058;
        theme.navItemTextArgb         = 0xFFE8EEF4;
        theme.dropdownBgArgb          = 0xFF202A35;
        theme.dropdownItemTextArgb    = 0xFFF3EAD7;
        theme.dropdownAccelArgb       = 0xFFB8C0CA;
        theme.dropdownHoverArgb       = 0xFF34475F;
        theme.driveBodyArgb           = 0xFF141414;
        theme.driveBezelArgb          = 0xFF050505;
        theme.driveLabelArgb          = 0xFFE6E2D8;
        theme.ledIdleArgb             = 0xFF1A0606;
        theme.ledPresentArgb          = 0xFF1A0606;
        theme.ledActiveArgb           = 0xFFFF2818;
        theme.ledHaloArgb             = 0x60FF2818;
        return theme;
    }


    static ChromeTheme DarkModern()
    {
        ChromeTheme  theme = {};



        // Win11-style flat dark: graphite gradient, near-pure-white
        // labels, cool blue accent for hover/highlight, blue LEDs.
        theme.titleBarTopArgb               = 0xFF202225;
        theme.titleBarBottomArgb            = 0xFF17181B;
        theme.titleTextArgb                 = 0xFFF0F0F0;
        theme.sysButtonIdleArgb             = 0x00000000;
        theme.sysButtonHoverArgb            = 0x0FFFFFFF;
        theme.sysButtonPressedArgb          = 0x0AFFFFFF;
        theme.sysButtonCloseHoverArgb       = 0xFFC42B1C;
        theme.sysButtonCloseHoverGlyphArgb  = 0xFFFFFFFF;
        theme.sysButtonClosePressedArgb     = 0xFFC42B1C;
        theme.navStripArgb                  = 0xFF2A2D32;
        theme.navHoverArgb                  = 0xFF3D6FB5;
        theme.navItemTextArgb               = 0xFFF0F0F0;
        theme.dropdownBgArgb                = 0xFF26282C;
        theme.dropdownItemTextArgb          = 0xFFF0F0F0;
        theme.dropdownAccelArgb             = 0xFFB8C0CA;
        theme.dropdownHoverArgb             = 0xFF3D6FB5;
        theme.driveBodyArgb                 = 0xFF1A1C1F;
        theme.driveBezelArgb                = 0xFF050505;
        theme.driveLabelArgb                = 0xFFE6E2D8;
        theme.ledIdleArgb                   = 0xFF06121A;
        theme.ledPresentArgb                = 0xFF06121A;
        theme.ledActiveArgb                 = 0xFF3DA1FF;
        theme.ledHaloArgb                   = 0x603DA1FF;
        return theme;
    }


    static ChromeTheme RetroTerminal()
    {
        ChromeTheme  theme = {};



        // P1 phosphor on near-black: deep green-black panels with
        // bright phosphor-green labels and LEDs to evoke a CRT cabinet.
        theme.titleBarTopArgb               = 0xFF16381E;
        theme.titleBarBottomArgb            = 0xFF0A1F0E;
        theme.titleTextArgb                 = 0xFFB7FCB9;
        theme.sysButtonIdleArgb             = 0x00000000;
        theme.sysButtonHoverArgb            = 0x14B7FCB9;
        theme.sysButtonPressedArgb          = 0x0AB7FCB9;
        theme.sysButtonCloseHoverArgb       = 0xFFC42B1C;
        theme.sysButtonCloseHoverGlyphArgb  = 0xFFFFFFFF;
        theme.sysButtonClosePressedArgb     = 0xFFC42B1C;
        theme.navStripArgb                  = 0xFF102714;
        theme.navHoverArgb                  = 0xFF1F4A28;
        theme.navItemTextArgb               = 0xFFB7FCB9;
        theme.dropdownBgArgb                = 0xFF12301A;
        theme.dropdownItemTextArgb          = 0xFFB7FCB9;
        theme.dropdownAccelArgb             = 0xFF6DA875;
        theme.dropdownHoverArgb             = 0xFF1F4A28;
        theme.driveBodyArgb                 = 0xFF0F1A12;
        theme.driveBezelArgb                = 0xFF040A05;
        theme.driveLabelArgb                = 0xFFB7FCB9;
        theme.ledIdleArgb                   = 0xFF071907;
        theme.ledPresentArgb                = 0xFF071907;
        theme.ledActiveArgb                 = 0xFF2BFF6A;
        theme.ledHaloArgb                   = 0x602BFF6A;
        return theme;
    }


    static ChromeTheme ForName (const std::string & name)
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
