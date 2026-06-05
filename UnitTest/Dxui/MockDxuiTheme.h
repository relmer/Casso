#pragma once

#include "Pch.h"



////////////////////////////////////////////////////////////////////////////////
//
//  MockDxuiTheme
//
//  Deterministic, in-memory `IDxuiTheme` implementation for unit tests.
//  Every accessor returns a canned ARGB or metric value so widget tests
//  can assert against fixed colours without depending on Casso's
//  ChromeTheme palette evolution.
//
//  Font handles return a null `DxuiFontHandle{}`; tests that need real
//  font measurement must construct fonts via the text renderer
//  directly. Paint paths are not exercised in this phase (no painter
//  mock yet) so the null handle is safe.
//
////////////////////////////////////////////////////////////////////////////////

class MockDxuiTheme : public IDxuiTheme
{
public:
    uint32_t  Background          () const override { return s_kBackground;          }
    uint32_t  BackgroundElevated  () const override { return s_kBackgroundElevated;  }
    uint32_t  HoverBackground     () const override { return s_kHoverBackground;     }
    uint32_t  PressedBackground   () const override { return s_kPressedBackground;   }
    uint32_t  SelectionBackground () const override { return s_kSelectionBackground; }

    uint32_t  Foreground          () const override { return s_kForeground;          }
    uint32_t  ForegroundMuted     () const override { return s_kForegroundMuted;     }
    uint32_t  ForegroundDisabled  () const override { return s_kForegroundDisabled;  }
    uint32_t  HeadingForeground   () const override { return s_kHeadingForeground;   }

    uint32_t  Accent              () const override { return s_kAccent;              }
    uint32_t  FocusRing           () const override { return s_kFocusRing;           }
    uint32_t  Border              () const override { return s_kBorder;              }
    uint32_t  Divider             () const override { return s_kDivider;             }

    uint32_t  ButtonIdle          () const override { return s_kButtonIdle;          }
    uint32_t  ButtonHover         () const override { return s_kButtonHover;         }
    uint32_t  ButtonPressed       () const override { return s_kButtonPressed;       }
    uint32_t  ButtonBorder        () const override { return s_kButtonBorder;        }
    uint32_t  ButtonText          () const override { return s_kButtonText;          }

    uint32_t  CaptionBackground   () const override { return s_kCaptionBackground;   }
    uint32_t  CaptionForeground   () const override { return s_kCaptionForeground;   }
    uint32_t  TitleBarTop         () const override { return s_kTitleBarTop;         }
    uint32_t  TitleBarBottom      () const override { return s_kTitleBarBottom;      }
    uint32_t  SystemButtonHover   () const override { return s_kSystemButtonHover;   }
    uint32_t  SystemButtonPressed () const override { return s_kSystemButtonPressed; }
    uint32_t  SystemCloseHover    () const override { return s_kSystemCloseHover;    }
    uint32_t  SystemClosePressed  () const override { return s_kSystemClosePressed;  }

    DxuiFontHandle  BodyFont      () const override { return {}; }
    DxuiFontHandle  BodyBoldFont  () const override { return {}; }
    DxuiFontHandle  CaptionFont   () const override { return {}; }
    DxuiFontHandle  HeadingFont   () const override { return {}; }
    DxuiFontHandle  MonospaceFont () const override { return {}; }

    float  BodyLineHeightDip () const override { return s_kBodyLineHeightDip; }
    float  CornerRadiusDip   () const override { return s_kCornerRadiusDip;   }
    float  FocusRingWidthDip () const override { return s_kFocusRingWidthDip; }


    // Canned values exposed publicly so tests can assert them.
    static constexpr uint32_t  s_kBackground          = 0xFF101010;
    static constexpr uint32_t  s_kBackgroundElevated  = 0xFF202020;
    static constexpr uint32_t  s_kHoverBackground     = 0xFF303030;
    static constexpr uint32_t  s_kPressedBackground   = 0xFF404040;
    static constexpr uint32_t  s_kSelectionBackground = 0xFF505050;
    static constexpr uint32_t  s_kForeground          = 0xFFEEEEEE;
    static constexpr uint32_t  s_kForegroundMuted     = 0xFFAAAAAA;
    static constexpr uint32_t  s_kForegroundDisabled  = 0x80EEEEEE;
    static constexpr uint32_t  s_kHeadingForeground   = 0xFFFFFFFF;
    static constexpr uint32_t  s_kAccent              = 0xFF3D6FB5;
    static constexpr uint32_t  s_kFocusRing           = 0xFF3D6FB5;
    static constexpr uint32_t  s_kBorder              = 0xFF606060;
    static constexpr uint32_t  s_kDivider             = 0xFF707070;
    static constexpr uint32_t  s_kButtonIdle          = 0xFF323539;
    static constexpr uint32_t  s_kButtonHover         = 0xFF45494F;
    static constexpr uint32_t  s_kButtonPressed       = 0xFF23252A;
    static constexpr uint32_t  s_kButtonBorder        = 0xFF55595F;
    static constexpr uint32_t  s_kButtonText          = 0xFFF0F0F0;
    static constexpr uint32_t  s_kCaptionBackground   = 0xFF181818;
    static constexpr uint32_t  s_kCaptionForeground   = 0xFFF0F0F0;
    static constexpr uint32_t  s_kTitleBarTop         = 0xFF202225;
    static constexpr uint32_t  s_kTitleBarBottom      = 0xFF17181B;
    static constexpr uint32_t  s_kSystemButtonHover   = 0xFF606060;
    static constexpr uint32_t  s_kSystemButtonPressed = 0xFF505050;
    static constexpr uint32_t  s_kSystemCloseHover    = 0xFFC42B1C;
    static constexpr uint32_t  s_kSystemClosePressed  = 0xFFB02014;

    static constexpr float  s_kBodyLineHeightDip = 18.0f;
    static constexpr float  s_kCornerRadiusDip   = 4.0f;
    static constexpr float  s_kFocusRingWidthDip = 2.0f;
};
