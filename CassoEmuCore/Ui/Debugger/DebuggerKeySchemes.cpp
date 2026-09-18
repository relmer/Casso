#include "Pch.h"

#include "Ui/Debugger/DebuggerKeySchemes.h"





using Action = DebuggerKeySchemes::Action;

static constexpr DxuiKeyChord  s_kVisualStudioKeys[] =
{
    { VK_F5,  false, false, false, (int) Action::Run              },
    { VK_F5,  false, false, true,  (int) Action::Pause            },
    { VK_F11, false, false, false, (int) Action::StepInto         },
    { VK_F10, false, false, false, (int) Action::StepOver         },
    { VK_F11, false, false, true,  (int) Action::StepOut          },
    { VK_F9,  false, false, false, (int) Action::ToggleBreakpoint },
    { VK_F10, true,  false, false, (int) Action::RunToCursor      },
};

static constexpr DxuiKeyChord  s_kAppleWinKeys[] =
{
    { VK_RETURN, false, false, false, (int) Action::Run              },
    { VK_F5,     false, false, true,  (int) Action::Pause            },
    { VK_SPACE,  false, false, false, (int) Action::StepInto         },
    { VK_SPACE,  true,  false, false, (int) Action::StepOver         },
    { VK_SPACE,  false, false, true,  (int) Action::StepOut          },
    { VK_F9,     false, false, false, (int) Action::ToggleBreakpoint },
    { VK_DOWN,   true,  false, false, (int) Action::RunToCursor      },
};

static constexpr DxuiKeyChord  s_kGSSquaredKeys[] =
{
    { VK_RETURN, false, false, false, (int) Action::Run              },
    { VK_F5,     false, false, true,  (int) Action::Pause            },
    { VK_SPACE,  false, false, false, (int) Action::StepInto         },
    { 'O',       false, false, false, (int) Action::StepOver         },
    { 'R',       false, false, false, (int) Action::StepOut          },
    { VK_F9,     false, false, false, (int) Action::ToggleBreakpoint },
    { VK_F10,    true,  false, false, (int) Action::RunToCursor      },
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeySchemes::GetMap
//
//  Built once each on first use and never freed, so a window can hold the
//  pointer for as long as it lives.
//
////////////////////////////////////////////////////////////////////////////////

const DxuiKeyMap & DebuggerKeySchemes::GetMap (DebuggerKeyScheme scheme)
{
    static const DxuiKeyMap  s_visualStudio (L"Visual Studio", s_kVisualStudioKeys);
    static const DxuiKeyMap  s_appleWin     (L"AppleWin",      s_kAppleWinKeys);
    static const DxuiKeyMap  s_gsSquared    (L"GSSquared",     s_kGSSquaredKeys);



    switch (scheme)
    {
    case DebuggerKeyScheme::AppleWin:  return s_appleWin;
    case DebuggerKeyScheme::GSSquared: return s_gsSquared;
    default:                           return s_visualStudio;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeySchemes::GetName
//
//  The name the preference is stored under.
//
////////////////////////////////////////////////////////////////////////////////

std::string DebuggerKeySchemes::GetName (DebuggerKeyScheme scheme)
{
    switch (scheme)
    {
    case DebuggerKeyScheme::AppleWin:  return "AppleWin";
    case DebuggerKeyScheme::GSSquared: return "GSSquared";
    default:                           return "VisualStudio";
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeySchemes::TryParse
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerKeySchemes::TryParse (const std::string & name, DebuggerKeyScheme & scheme)
{
    for (DebuggerKeyScheme each : { DebuggerKeyScheme::VisualStudio, DebuggerKeyScheme::AppleWin, DebuggerKeyScheme::GSSquared })
    {
        std::string  candidate = GetName (each);

        if (candidate.size() == name.size() &&
            std::equal (candidate.begin(), candidate.end(), name.begin(),
                        [] (char a, char b) { return std::tolower ((unsigned char) a) == std::tolower ((unsigned char) b); }))
        {
            scheme = each;
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeySchemes::DoesBoxKeepKey
//
//  An empty box has nothing a Space or Enter could mean, which is why AppleWin
//  steps on Space only from an empty line. A letter is kept even from an empty
//  box: it is the first letter of a command.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerKeySchemes::DoesBoxKeepKey (WPARAM vk, bool ctrl, bool alt, bool boxFocused, bool boxEmpty)
{
    bool  isFunctionKey = vk >= VK_F1 && vk <= VK_F24;
    bool  keeps         = true;



    if (!boxFocused || ctrl || alt || isFunctionKey)
    {
        keeps = false;
    }
    else if (vk == VK_SPACE || vk == VK_RETURN)
    {
        keeps = !boxEmpty;
    }

    return keeps;
}
