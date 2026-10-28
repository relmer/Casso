#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTextStyle
//
//  A text style for one use in Casso Explorer. Apply sets only the fields
//  that have a value.
//
////////////////////////////////////////////////////////////////////////////////

struct ExplorerTextStyle
{
public:
    std::optional<DxuiTextRole>    role;
    std::optional<float>           fontDip;    // 0 is the control's default size
    std::optional<DxuiFontWeight>  weight;
    std::optional<float>           strength;   // from the background toward the theme's foreground
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTextStyles
//
//  Casso Explorer's text styles, by use, and their application to any Dxui
//  control that has the style's settings.
//
////////////////////////////////////////////////////////////////////////////////

class ExplorerTextStyles
{
public:
    static constexpr ExplorerTextStyle  kProse           { .fontDip  = 12.0f };   // the tree's and list's rows: Explorer's 9 points
    static constexpr ExplorerTextStyle  kListing         { .fontDip  = 0.0f };    // a catalog's rows in the preview
    static constexpr ExplorerTextStyle  kPreviewText     { .strength = 0.8f };    // a gray like a terminal's text rather than full white
    static constexpr ExplorerTextStyle  kPanePrompt      { .role     = DxuiTextRole::Body,  .fontDip = 12.0f };   // Explorer's smaller body text
    static constexpr ExplorerTextStyle  kPaneMessage     { .role     = DxuiTextRole::Muted, .fontDip = 0.0f };
    static constexpr ExplorerTextStyle  kFormLabel       { .role     = DxuiTextRole::Body };
    static constexpr ExplorerTextStyle  kFormNote        { .role     = DxuiTextRole::Muted };
    static constexpr ExplorerTextStyle  kPropertiesName  { .role     = DxuiTextRole::Heading, .weight = DxuiFontWeight::SemiBold };
    static constexpr ExplorerTextStyle  kPropertiesLabel { .role     = DxuiTextRole::Muted };
    static constexpr ExplorerTextStyle  kPropertiesValue { .role     = DxuiTextRole::Body };

    template <typename T>
    static void  Apply (T & control, const ExplorerTextStyle & style);
};





////////////////////////////////////////////////////////////////////////////////
//
//  ExplorerTextStyles::Apply
//
////////////////////////////////////////////////////////////////////////////////

template <typename T>
void ExplorerTextStyles::Apply (T & control, const ExplorerTextStyle & style)
{
    if constexpr (requires { control.SetTextRole (DxuiTextRole::Body); })
    {
        if (style.role.has_value())
        {
            control.SetTextRole (*style.role);
        }
    }

    if constexpr (requires { control.SetFontDip (0.0f); })
    {
        if (style.fontDip.has_value())
        {
            control.SetFontDip (*style.fontDip);
        }
    }

    if constexpr (requires { control.SetFontWeight (DxuiFontWeight::Normal); })
    {
        if (style.weight.has_value())
        {
            control.SetFontWeight (*style.weight);
        }
    }

    if constexpr (requires { control.SetTextStrength (0.0f); })
    {
        if (style.strength.has_value())
        {
            control.SetTextStrength (*style.strength);
        }
    }
}
