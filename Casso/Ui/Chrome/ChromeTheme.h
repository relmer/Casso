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
    uint32_t  sysButtonCloseHoverArgb = 0;
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
        theme.sysButtonIdleArgb       = 0x00101824;
        theme.sysButtonHoverArgb      = 0x334D6A86;
        theme.sysButtonPressedArgb    = 0x665E7C99;
        theme.sysButtonCloseHoverArgb = 0xFFD83B3B;
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
