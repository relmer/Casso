#pragma once

#include "Pch.h"
#include "IDxuiTheme.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DxuiTheme
//
//  Concrete IDxuiTheme: a plain bag of packed-ARGB tokens for every
//  Dxui-rendered surface (panels, buttons, caption / system buttons, menu
//  strip, dropdowns, links) plus typography and metrics, with the
//  IDxuiTheme accessors mapping straight onto those tokens. A host extends
//  this with its own derived theme (e.g. CassoTheme adds drive / LED
//  tokens) and supplies presets; widgets depend only on IDxuiTheme.
//
//  Tokens are public so a host widget can read a specific named colour
//  (link, navHover, …) directly while generic Dxui widgets stay on
//  the interface accessors. Dark() / Light() give a neutral starting set
//  any host can tweak.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTheme : public IDxuiTheme
{
    uint32_t  titleBarTop              = 0;
    uint32_t  titleBarBottom           = 0;
    uint32_t  titleText                = 0;
    uint32_t  sysButtonIdle            = 0;
    uint32_t  sysButtonHover           = 0;
    uint32_t  sysButtonPressed         = 0;
    uint32_t  sysButtonCloseHover      = 0;
    uint32_t  sysButtonCloseHoverGlyph = 0;
    uint32_t  sysButtonClosePressed    = 0;
    uint32_t  navStrip                 = 0;
    uint32_t  navHover                 = 0;
    uint32_t  navItemText              = 0;
    uint32_t  dropdownBg               = 0;
    uint32_t  dropdownItemText         = 0;
    uint32_t  dropdownAccel            = 0;
    uint32_t  dropdownHover            = 0;
    uint32_t  link                     = 0;
    uint32_t  linkHover                = 0;
    uint32_t  panelBg                  = 0;
    uint32_t  panelEdge                = 0;
    uint32_t  buttonIdle               = 0;
    uint32_t  buttonHover              = 0;
    uint32_t  buttonPressed            = 0;
    uint32_t  buttonBorder             = 0;
    uint32_t  tooltipBg                = 0;
    uint32_t  tooltipBorder            = 0;
    uint32_t  tooltipText              = 0;

    // IDxuiTheme overrides map the named tokens onto the generic contract
    // so any Dxui widget paints against this theme through the interface.
    uint32_t  Background          () const override { return panelBg;            }
    uint32_t  BackgroundElevated  () const override { return dropdownBg;         }
    uint32_t  HoverBackground     () const override { return navHover;           }
    uint32_t  PressedBackground   () const override { return buttonPressed;      }
    uint32_t  SelectionBackground () const override { return navHover;           }

    uint32_t  Foreground          () const override { return dropdownItemText;   }
    uint32_t  ForegroundMuted     () const override { return dropdownAccel;      }
    uint32_t  ForegroundDisabled  () const override
    {
        // Half-alpha primary foreground -- no dedicated disabled-text knob;
        // keeps visual parity with the prior in-widget disable mask.
        return (dropdownItemText & 0x00FFFFFFu) | 0x80000000u;
    }
    uint32_t  HeadingForeground   () const override { return titleText;          }

    uint32_t  Accent              () const override { return link;               }
    uint32_t  FocusRing           () const override { return link;               }
    uint32_t  Border              () const override { return panelEdge;          }
    uint32_t  Divider             () const override { return buttonBorder;       }

    uint32_t  ButtonIdle          () const override { return buttonIdle;         }
    uint32_t  ButtonHover         () const override { return buttonHover;        }
    uint32_t  ButtonPressed       () const override { return buttonPressed;      }
    uint32_t  ButtonBorder        () const override { return buttonBorder;       }
    uint32_t  ButtonText          () const override { return navItemText;        }

    uint32_t  CaptionBackground   () const override { return titleBarTop;        }
    uint32_t  CaptionForeground   () const override { return titleText;          }
    uint32_t  TitleBarTop         () const override { return titleBarTop;        }
    uint32_t  TitleBarBottom      () const override { return titleBarBottom;     }
    uint32_t  SystemButtonHover   () const override { return sysButtonHover;     }
    uint32_t  SystemButtonPressed () const override { return sysButtonPressed;   }
    uint32_t  SystemCloseHover    () const override { return sysButtonCloseHover; }
    uint32_t  SystemClosePressed  () const override { return sysButtonClosePressed; }

    uint32_t  TooltipBackground   () const override { return tooltipBg;     }
    uint32_t  TooltipBorder       () const override { return tooltipBorder; }
    uint32_t  TooltipForeground   () const override { return tooltipText;   }

    // Typography. Faces and sizes are centralized here so widgets read
    // theme fonts instead of repeating literals. Icon-glyph faces (Segoe
    // MDL2 Assets / Symbol) stay at the use site -- they are not text.
    static constexpr wchar_t       kBodyFace    [] = L"Segoe UI";
    static constexpr wchar_t       kMonoFace    [] = L"Cascadia Mono";
    static constexpr float         kBodySizeDip    = 13.0f;
    static constexpr float         kCaptionSizeDip = 12.0f;
    static constexpr float         kHeadingSizeDip = 14.0f;

    DxuiFontHandle  BodyFont      () const override { return { kBodyFace, kBodySizeDip,    DWRITE_FONT_WEIGHT_NORMAL    }; }
    DxuiFontHandle  BodyBoldFont  () const override { return { kBodyFace, kBodySizeDip,    DWRITE_FONT_WEIGHT_SEMI_BOLD }; }
    DxuiFontHandle  CaptionFont   () const override { return { kBodyFace, kCaptionSizeDip, DWRITE_FONT_WEIGHT_NORMAL    }; }
    DxuiFontHandle  HeadingFont   () const override { return { kBodyFace, kHeadingSizeDip, DWRITE_FONT_WEIGHT_NORMAL    }; }
    DxuiFontHandle  MonospaceFont () const override { return { kMonoFace, kBodySizeDip,    DWRITE_FONT_WEIGHT_NORMAL    }; }

    float  BodyLineHeightDip () const override { return 18.0f; }
    float  CornerRadiusDip   () const override { return 4.0f;  }
    float  FocusRingWidthDip () const override { return 2.0f;  }

    static DxuiTheme Dark()
    {
        DxuiTheme  theme = {};



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
        return theme;
    }


    static DxuiTheme Light()
    {
        DxuiTheme  theme = {};



        theme.titleBarTop               = 0xFFF3F3F3;
        theme.titleBarBottom            = 0xFFE6E6E6;
        theme.titleText                 = 0xFF1A1A1A;
        theme.sysButtonIdle             = 0x00000000;
        theme.sysButtonHover            = 0x14000000;
        theme.sysButtonPressed          = 0x0A000000;
        theme.sysButtonCloseHover       = 0xFFC42B1C;
        theme.sysButtonCloseHoverGlyph  = 0xFFFFFFFF;
        theme.sysButtonClosePressed     = 0xFFC42B1C;
        theme.navStrip                  = 0xFFEAEAEA;
        theme.navHover                  = 0xFFCFE2FF;
        theme.navItemText               = 0xFF1A1A1A;
        theme.dropdownBg                = 0xFFFBFBFB;
        theme.dropdownItemText          = 0xFF1A1A1A;
        theme.dropdownAccel             = 0xFF606060;
        theme.dropdownHover             = 0xFFCFE2FF;
        theme.link                      = 0xFF1A6FD0;
        theme.linkHover                 = 0xFF0A4FA0;
        theme.panelBg                   = 0xFFF6F6F6;
        theme.panelEdge                 = 0xFFC8C8C8;
        theme.buttonIdle                = 0xFFE0E0E0;
        theme.buttonHover               = 0xFFD0D0D0;
        theme.buttonPressed             = 0xFFC0C0C0;
        theme.buttonBorder              = 0xFFB0B0B0;
        theme.tooltipBg                 = 0xFFFFFFFF;
        theme.tooltipBorder             = 0xFFB0B0B0;
        theme.tooltipText               = 0xFF1A1A1A;
        return theme;
    }
};
