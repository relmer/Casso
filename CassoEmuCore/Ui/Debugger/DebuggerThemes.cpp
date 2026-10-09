#include "Pch.h"

#include "Ui/Debugger/DebuggerThemes.h"





static constexpr const char *  s_kpszSkeuomorphic  = "Skeuomorphic";
static constexpr const char *  s_kpszDarkModern    = "DarkModern";
static constexpr const char *  s_kpszRetroTerminal = "RetroTerminal";





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerThemes::GetChoices
//
//  The Casso theme names are the ones CassoTheme::MakeByName reads, so the
//  emulator, Casso Explorer and the debugger save the same text.
//
////////////////////////////////////////////////////////////////////////////////

const std::vector<DebuggerThemes::Choice> & DebuggerThemes::GetChoices()
{
    static const std::vector<Choice>  choices =
    {
        { kSameAsCasso,        L"Same as Casso"        },
        { s_kpszSkeuomorphic,  L"Casso Skeuomorphic"   },
        { s_kpszDarkModern,    L"Casso Dark Modern"    },
        { s_kpszRetroTerminal, L"Casso Retro Terminal" },
        { kSystemLight,        L"System light"         },
        { kSystemDark,         L"System dark"          },
    };



    return choices;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerThemes::IsKnown
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerThemes::IsKnown (const std::string & name)
{
    for (const Choice & choice : GetChoices())
    {
        if (!name.empty() && name == choice.name)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerThemes::ApplyOwnColors
//
//  Measured from Visual Studio 2026 at 125% on 2026-10-08.
//
//  Its tooltips: #2C2C2C bordered in #1C1C1C with white text in dark, and
//  #F9F9F9 bordered in #DDDDDD with #212121 text in light.
//
//  Its light panes: lists and titles #F9F9F9, text views #FFFFFF, outlined
//  in #ADADAD, and #5649B0 around the focused one. The band behind the tabs
//  and the gap between panes derive from the content color. The lines inside
//  a list keep the light theme's #E5E5E5, which the outline no longer gives
//  them.
//
//  The dark panes keep the system's content color, as Casso Explorer's do.
//
////////////////////////////////////////////////////////////////////////////////

void DebuggerThemes::ApplyOwnColors (
    DxuiLightTheme & light,
    DxuiDarkTheme  & dark)
{
    constexpr uint32_t  kLightLine = 0xFFE5E5E5;



    dark.tooltipBg           = 0xFF2C2C2C;
    dark.tooltipBorder       = 0xFF1C1C1C;
    dark.tooltipText         = 0xFFFFFFFF;

    light.tooltipBg          = 0xFFF9F9F9;
    light.tooltipBorder      = 0xFFDDDDDD;
    light.tooltipText        = 0xFF212121;
    light.contentBg          = 0xFFF9F9F9;
    light.textViewBg         = 0xFFFFFFFF;
    light.panelEdge          = 0xFFADADAD;
    light.focusAccent        = 0xFF5649B0;
    light.contentEdge        = kLightLine;
    light.splitterHighlight  = kLightLine;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerThemes::Choose
//
////////////////////////////////////////////////////////////////////////////////

const DxuiTheme & DebuggerThemes::Choose (const std::string    & name,
                                          const DxuiTheme      & emulator,
                                          const DxuiLightTheme & light,
                                          const DxuiDarkTheme  & dark,
                                          CassoTheme           & own)
{
    if (name == kSystemLight)
    {
        return light;
    }

    if (name == kSystemDark)
    {
        return dark;
    }

    if (!IsKnown (name))
    {
        return emulator;
    }

    own = CassoTheme::MakeByName (name);
    return own;
}
