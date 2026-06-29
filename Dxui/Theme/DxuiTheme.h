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
//  (linkArgb, navHoverArgb, …) directly while generic Dxui widgets stay on
//  the interface accessors. Dark() / Light() give a neutral starting set
//  any host can tweak.
//
////////////////////////////////////////////////////////////////////////////////

struct DxuiTheme : public IDxuiTheme
{
    uint32_t  titleBarTopArgb              = 0;
    uint32_t  titleBarBottomArgb           = 0;
    uint32_t  titleTextArgb                = 0;
    uint32_t  sysButtonIdleArgb            = 0;
    uint32_t  sysButtonHoverArgb           = 0;
    uint32_t  sysButtonPressedArgb         = 0;
    uint32_t  sysButtonCloseHoverArgb      = 0;
    uint32_t  sysButtonCloseHoverGlyphArgb = 0;
    uint32_t  sysButtonClosePressedArgb    = 0;
    uint32_t  navStripArgb                 = 0;
    uint32_t  navHoverArgb                 = 0;
    uint32_t  navItemTextArgb              = 0;
    uint32_t  dropdownBgArgb               = 0;
    uint32_t  dropdownItemTextArgb         = 0;
    uint32_t  dropdownAccelArgb            = 0;
    uint32_t  dropdownHoverArgb            = 0;
    uint32_t  linkArgb                     = 0;
    uint32_t  linkHoverArgb                = 0;
    uint32_t  panelBgArgb                  = 0;
    uint32_t  panelEdgeArgb                = 0;
    uint32_t  buttonIdleArgb               = 0;
    uint32_t  buttonHoverArgb              = 0;
    uint32_t  buttonPressedArgb            = 0;
    uint32_t  buttonBorderArgb             = 0;

    // IDxuiTheme overrides map the named tokens onto the generic contract
    // so any Dxui widget paints against this theme through the interface.
    uint32_t  Background          () const override { return panelBgArgb;            }
    uint32_t  BackgroundElevated  () const override { return dropdownBgArgb;         }
    uint32_t  HoverBackground     () const override { return navHoverArgb;           }
    uint32_t  PressedBackground   () const override { return buttonPressedArgb;      }
    uint32_t  SelectionBackground () const override { return navHoverArgb;           }

    uint32_t  Foreground          () const override { return dropdownItemTextArgb;   }
    uint32_t  ForegroundMuted     () const override { return dropdownAccelArgb;      }
    uint32_t  ForegroundDisabled  () const override
    {
        // Half-alpha primary foreground -- no dedicated disabled-text knob;
        // keeps visual parity with the prior in-widget disable mask.
        return (dropdownItemTextArgb & 0x00FFFFFFu) | 0x80000000u;
    }
    uint32_t  HeadingForeground   () const override { return titleTextArgb;          }

    uint32_t  Accent              () const override { return linkArgb;               }
    uint32_t  FocusRing           () const override { return linkArgb;               }
    uint32_t  Border              () const override { return panelEdgeArgb;          }
    uint32_t  Divider             () const override { return buttonBorderArgb;       }

    uint32_t  ButtonIdle          () const override { return buttonIdleArgb;         }
    uint32_t  ButtonHover         () const override { return buttonHoverArgb;        }
    uint32_t  ButtonPressed       () const override { return buttonPressedArgb;      }
    uint32_t  ButtonBorder        () const override { return buttonBorderArgb;       }
    uint32_t  ButtonText          () const override { return navItemTextArgb;        }

    uint32_t  CaptionBackground   () const override { return titleBarTopArgb;        }
    uint32_t  CaptionForeground   () const override { return titleTextArgb;          }
    uint32_t  TitleBarTop         () const override { return titleBarTopArgb;        }
    uint32_t  TitleBarBottom      () const override { return titleBarBottomArgb;     }
    uint32_t  SystemButtonHover   () const override { return sysButtonHoverArgb;     }
    uint32_t  SystemButtonPressed () const override { return sysButtonPressedArgb;   }
    uint32_t  SystemCloseHover    () const override { return sysButtonCloseHoverArgb; }
    uint32_t  SystemClosePressed  () const override { return sysButtonClosePressedArgb; }

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
        theme.linkArgb                      = 0xFF6FB8FF;
        theme.linkHoverArgb                 = 0xFFA8D2FF;
        theme.panelBgArgb                   = 0xFF1E2024;
        theme.panelEdgeArgb                 = 0xFF3A3D42;
        theme.buttonIdleArgb                = 0xFF323539;
        theme.buttonHoverArgb               = 0xFF45494F;
        theme.buttonPressedArgb             = 0xFF23252A;
        theme.buttonBorderArgb              = 0xFF55595F;
        return theme;
    }


    static DxuiTheme Light()
    {
        DxuiTheme  theme = {};



        theme.titleBarTopArgb               = 0xFFF3F3F3;
        theme.titleBarBottomArgb            = 0xFFE6E6E6;
        theme.titleTextArgb                 = 0xFF1A1A1A;
        theme.sysButtonIdleArgb             = 0x00000000;
        theme.sysButtonHoverArgb            = 0x14000000;
        theme.sysButtonPressedArgb          = 0x0A000000;
        theme.sysButtonCloseHoverArgb       = 0xFFC42B1C;
        theme.sysButtonCloseHoverGlyphArgb  = 0xFFFFFFFF;
        theme.sysButtonClosePressedArgb     = 0xFFC42B1C;
        theme.navStripArgb                  = 0xFFEAEAEA;
        theme.navHoverArgb                  = 0xFFCFE2FF;
        theme.navItemTextArgb               = 0xFF1A1A1A;
        theme.dropdownBgArgb                = 0xFFFBFBFB;
        theme.dropdownItemTextArgb          = 0xFF1A1A1A;
        theme.dropdownAccelArgb             = 0xFF606060;
        theme.dropdownHoverArgb             = 0xFFCFE2FF;
        theme.linkArgb                      = 0xFF1A6FD0;
        theme.linkHoverArgb                 = 0xFF0A4FA0;
        theme.panelBgArgb                   = 0xFFF6F6F6;
        theme.panelEdgeArgb                 = 0xFFC8C8C8;
        theme.buttonIdleArgb                = 0xFFE0E0E0;
        theme.buttonHoverArgb               = 0xFFD0D0D0;
        theme.buttonPressedArgb             = 0xFFC0C0C0;
        theme.buttonBorderArgb              = 0xFFB0B0B0;
        return theme;
    }
};
