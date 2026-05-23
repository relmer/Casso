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
        // Caption-button overlays match the Fluent/Win11 specification:
        // - idle is fully transparent so the title-bar gradient shows
        //   through; hover/pressed are theme-neutral white overlays
        //   at ~10% / ~16% alpha; close hover/pressed are the
        //   documented Windows red (#C42B1C) with a slightly darker
        //   pressed shade. Close hover also flips the glyph color
        //   to white per Windows convention so the X reads clearly
        //   against the red fill.
        theme.sysButtonIdleArgb          = 0x00000000;
        theme.sysButtonHoverArgb         = 0x1AFFFFFF;
        theme.sysButtonPressedArgb       = 0x29FFFFFF;
        theme.sysButtonCloseHoverArgb       = 0xFFC42B1C;
        theme.sysButtonCloseHoverGlyphArgb  = 0xFFFFFFFF;
        theme.sysButtonClosePressedArgb     = 0xFFB12414;
        theme.navStripArgb            = 0xFF182536;
        theme.navHoverArgb            = 0xFF2D4058;
        theme.navItemTextArgb         = 0xFFE8EEF4;
        theme.dropdownBgArgb          = 0xFF202A35;
        theme.dropdownItemTextArgb    = 0xFFF3EAD7;
        theme.dropdownAccelArgb       = 0xFFB8C0CA;
        theme.dropdownHoverArgb       = 0xFF34475F;
        theme.driveBodyArgb           = 0xFFCDBB91;
        theme.driveBezelArgb          = 0xFF3D3426;
        theme.driveLabelArgb          = 0xFF1C1812;
        theme.ledIdleArgb             = 0xFF303030;
        theme.ledPresentArgb          = 0xFFFFB13B;
        theme.ledActiveArgb           = 0xFF55FF74;
        theme.ledHaloArgb             = 0x8055FF74;
        return theme;
    }
};
