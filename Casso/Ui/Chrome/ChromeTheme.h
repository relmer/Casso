#pragma once

#include "Pch.h"





struct ChromeVisualState
{
    UINT     dpi        = 96;
    int64_t  nowMs      = 0;
    int      frameIndex = 0;
};


struct ChromeTheme : public IDxuiTheme
{
    // Whether the drive widgets use the compact paint path (small flat
    // card with label + LED). False = full skeuomorphic Apple Disk II
    // widgets. The drive-bar thickness contracts with this flag via
    // EmulatorShell's theme listener so the emulator pixel grid is
    // preserved across theme swaps.
    bool      compactDrives             = false;

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
    uint32_t  linkArgb               = 0;
    uint32_t  linkHoverArgb          = 0;
    uint32_t  panelBgArgb            = 0;
    uint32_t  panelEdgeArgb          = 0;
    uint32_t  buttonIdleArgb         = 0;
    uint32_t  buttonHoverArgb        = 0;
    uint32_t  buttonPressedArgb      = 0;
    uint32_t  buttonBorderArgb       = 0;

    // IDxuiTheme overrides. These map the Casso skeuomorphic palette
    // onto the generic Dxui theme contract so any Dxui widget can
    // paint against a `ChromeTheme` via the interface base.
    uint32_t  Background          () const override { return panelBgArgb;            }
    uint32_t  BackgroundElevated  () const override { return dropdownBgArgb;         }
    uint32_t  HoverBackground     () const override { return navHoverArgb;           }
    uint32_t  PressedBackground   () const override { return buttonPressedArgb;      }
    uint32_t  SelectionBackground () const override { return navHoverArgb;           }

    uint32_t  Foreground          () const override { return dropdownItemTextArgb;   }
    uint32_t  ForegroundMuted     () const override { return dropdownAccelArgb;      }
    uint32_t  ForegroundDisabled  () const override
    {
        // Half-alpha primary foreground -- ChromeTheme has no
        // dedicated disabled-text knob today; this default keeps
        // visual parity with the prior in-widget disable mask.
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

    // Font handles. ChromeTheme does not own font resources today --
    // the text renderer owns IDWriteTextFormat objects keyed off
    // font name + size. Returning a null handle is safe because no
    // Dxui widget currently invokes the font accessors; Phase 6
    // wires real font ownership through the theme.
    DxuiFontHandle  BodyFont      () const override { return {}; }
    DxuiFontHandle  BodyBoldFont  () const override { return {}; }
    DxuiFontHandle  CaptionFont   () const override { return {}; }
    DxuiFontHandle  HeadingFont   () const override { return {}; }
    DxuiFontHandle  MonospaceFont () const override { return {}; }

    // Metrics. Sensible defaults until Casso surfaces per-theme
    // overrides. `BodyLineHeightDip` feeds focus-manager row epsilon.
    float  BodyLineHeightDip () const override { return 18.0f; }
    float  CornerRadiusDip   () const override { return 4.0f;  }
    float  FocusRingWidthDip () const override { return 2.0f;  }

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
        theme.linkArgb                = 0xFF6FB8FF;
        theme.linkHoverArgb           = 0xFFB7DFFF;
        theme.panelBgArgb             = 0xFF1A2230;
        theme.panelEdgeArgb           = 0xFF334050;
        theme.buttonIdleArgb          = 0xFF2D3F58;
        theme.buttonHoverArgb         = 0xFF3D547A;
        theme.buttonPressedArgb       = 0xFF1F2C40;
        theme.buttonBorderArgb        = 0xFF4A5F80;
        return theme;
    }


    static ChromeTheme DarkModern()
    {
        ChromeTheme  theme = {};



        // Win11-style flat dark: graphite gradient, near-pure-white
        // labels, cool blue accent for hover/highlight, blue LEDs.
        theme.compactDrives                 = true;
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


    static ChromeTheme RetroTerminal()
    {
        ChromeTheme  theme = {};



        // P1 phosphor on near-black: deep green-black panels with
        // bright phosphor-green labels and LEDs to evoke a CRT cabinet.
        theme.compactDrives                 = true;
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
        theme.linkArgb                      = 0xFF8AFF8A;
        theme.linkHoverArgb                 = 0xFFB7FCB9;
        theme.panelBgArgb                   = 0xFF0E2612;
        theme.panelEdgeArgb                 = 0xFF2A5C30;
        theme.buttonIdleArgb                = 0xFF1A3F22;
        theme.buttonHoverArgb               = 0xFF286036;
        theme.buttonPressedArgb             = 0xFF0F2814;
        theme.buttonBorderArgb              = 0xFF3A7548;
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
