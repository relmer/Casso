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
