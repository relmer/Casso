#include "Pch.h"

#include "Ui/Debugger/DebuggerKeySchemes.h"
#include "Ui/Debugger/DebuggerLayout.h"





using Action = DebuggerKeySchemes::Action;

//  Visual Studio's step back chords start with Ctrl+R.
static constexpr DxuiKeyStroke  s_kChordPrefix = { 'R', true, false, false };

//  The first keys of the pane chords: Visual Studio's watch and memory
//  windows, and Casso's event logs and BASIC panes.
static constexpr DxuiKeyStroke  s_kWatchPrefix  = { 'W', true, true, false };
static constexpr DxuiKeyStroke  s_kMemoryPrefix = { 'M', true, true, false };
static constexpr DxuiKeyStroke  s_kLogPrefix    = { 'L', true, true, false };
static constexpr DxuiKeyStroke  s_kBasicPrefix  = { 'P', true, true, false };

//  The pane keys, the same in every scheme: Visual Studio's for the windows
//  it has (Ctrl+Alt+I is its Immediate window, the console's nearest), and
//  Ctrl+Alt keys and chords of the same kind for the panes it lacks.
static constexpr DxuiKeyChord  s_kPaneKeys[] =
{
    { 'D',      true,  true,  false, (int) Action::ShowDisassembly    },
    { 'B',      true,  true,  false, (int) Action::ShowBreakpoints    },
    { 'C',      true,  true,  false, (int) Action::ShowCallStack      },
    { '1',      false, false, false, (int) Action::ShowWatches,        s_kWatchPrefix  },
    { '1',      false, false, false, (int) Action::ShowMemory1,        s_kMemoryPrefix },
    { '2',      false, false, false, (int) Action::ShowMemory2,        s_kMemoryPrefix },
    { '3',      false, false, false, (int) Action::ShowMemory3,        s_kMemoryPrefix },
    { '4',      false, false, false, (int) Action::ShowMemory4,        s_kMemoryPrefix },
    { 'G',      true,  true,  false, (int) Action::ShowRegisters      },
    { VK_OEM_3, true,  false, false, (int) Action::ShowConsole        },
    { 'I',      true,  true,  false, (int) Action::ShowConsole        },
    { 'K',      true,  true,  false, (int) Action::ShowStack          },
    { 'T',      true,  true,  false, (int) Action::ShowTrace          },
    { 'H',      true,  true,  false, (int) Action::ShowHeatMap        },
    { 'R',      true,  true,  false, (int) Action::ShowHeatRanges     },
    { 'D',      false, false, false, (int) Action::ShowDiskLog,        s_kLogPrefix    },
    { 'I',      false, false, false, (int) Action::ShowInputLog,       s_kLogPrefix    },
    { 'P',      false, false, false, (int) Action::ShowBasicProgram,   s_kBasicPrefix  },
    { 'V',      false, false, false, (int) Action::ShowBasicVariables, s_kBasicPrefix  },
    { 'S',      false, false, false, (int) Action::ShowBasicStack,     s_kBasicPrefix  },
};

//  Each pane action's pane, in the actions' order from ShowDisassembly.
static constexpr const wchar_t *  s_kpszPanes[] =
{
    DebuggerLayout::kCode, DebuggerLayout::kBreakpoints, DebuggerLayout::kCallStack, DebuggerLayout::kWatches,
    L"memory1", L"memory2", L"memory3", L"memory4",
    DebuggerLayout::kRegisters, DebuggerLayout::kConsole, DebuggerLayout::kStack, DebuggerLayout::kTrace,
    DebuggerLayout::kHeatMap, DebuggerLayout::kHeatRanges, DebuggerLayout::kDiskLog, DebuggerLayout::kInputLog,
    DebuggerLayout::kBasicProgram, DebuggerLayout::kBasicVariables, DebuggerLayout::kBasicStack,
};

static constexpr DxuiKeyChord  s_kVisualStudioKeys[] =
{
    { VK_F5,     false, false, false, (int) Action::Run              },
    { VK_F5,     false, false, true,  (int) Action::Pause            },
    { VK_CANCEL, true,  false, false, (int) Action::Pause            },
    { VK_F11,    false, false, false, (int) Action::StepInto         },
    { VK_F10,    false, false, false, (int) Action::StepOver         },
    { VK_F11,    false, false, true,  (int) Action::StepOut          },
    { VK_F9,     false, false, false, (int) Action::ToggleBreakpoint },
    { VK_F10,    true,  false, false, (int) Action::RunToCursor      },
    { 'F',       true,  false, false, (int) Action::Find             },
    { VK_F3,     false, false, false, (int) Action::FindNext         },
    { VK_F3,     false, false, true,  (int) Action::FindPrevious     },
    { VK_F6,     false, false, false, (int) Action::RunFrame         },
    { VK_F11,    false, true,  false, (int) Action::StepBackInto     },
    { VK_F11,    false, false, false, (int) Action::StepBackInto,     s_kChordPrefix },
    { VK_F10,    false, true,  false, (int) Action::StepBackOver     },
    { VK_F10,    false, false, false, (int) Action::StepBackOver,     s_kChordPrefix },
    { VK_F11,    false, true,  true,  (int) Action::StepBackOut      },
    { VK_F11,    false, false, true,  (int) Action::StepBackOut,      s_kChordPrefix },
    { VK_F5,     false, true,  false, (int) Action::ReverseRun       },
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
    { 'F',       true,  false, false, (int) Action::Find             },
    { VK_F3,     false, false, false, (int) Action::FindNext         },
    { VK_F3,     false, false, true,  (int) Action::FindPrevious     },
    { VK_F6,     false, false, false, (int) Action::RunFrame         },
    { VK_SPACE,  false, true,  false, (int) Action::StepBackInto     },
    { VK_SPACE,  true,  true,  false, (int) Action::StepBackOver     },
    { VK_SPACE,  false, true,  true,  (int) Action::StepBackOut      },
    { VK_RETURN, false, true,  false, (int) Action::ReverseRun       },
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
    { 'F',       true,  false, false, (int) Action::Find             },
    { VK_F3,     false, false, false, (int) Action::FindNext         },
    { VK_F3,     false, false, true,  (int) Action::FindPrevious     },
    { VK_F6,     false, false, false, (int) Action::RunFrame         },
    { VK_SPACE,  false, true,  false, (int) Action::StepBackInto     },
    { 'O',       false, true,  false, (int) Action::StepBackOver     },
    { 'R',       false, true,  false, (int) Action::StepBackOut      },
    { VK_RETURN, false, true,  false, (int) Action::ReverseRun       },
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
    static const DxuiKeyMap  s_visualStudio (L"Visual Studio", AddPaneKeys (s_kVisualStudioKeys));
    static const DxuiKeyMap  s_appleWin     (L"AppleWin",      AddPaneKeys (s_kAppleWinKeys));
    static const DxuiKeyMap  s_gsSquared    (L"GSSquared",     AddPaneKeys (s_kGSSquaredKeys));



    switch (scheme)
    {
    case DebuggerKeyScheme::AppleWin:  return s_appleWin;
    case DebuggerKeyScheme::GSSquared: return s_gsSquared;
    default:                           return s_visualStudio;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeySchemes::AddPaneKeys
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DxuiKeyChord> DebuggerKeySchemes::AddPaneKeys (std::span<const DxuiKeyChord> keys)
{
    std::vector<DxuiKeyChord>  chords (keys.begin(), keys.end());



    chords.insert (chords.end(), std::begin (s_kPaneKeys), std::end (s_kPaneKeys));
    return chords;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeySchemes::GetPaneOfAction
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DebuggerKeySchemes::GetPaneOfAction (Action action)
{
    int  index = (int) action - (int) Action::ShowDisassembly;



    if (index < 0 || index >= (int) std::size (s_kpszPanes))
    {
        return {};
    }

    return s_kpszPanes[index];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeySchemes::GetActionOfPane
//
////////////////////////////////////////////////////////////////////////////////

std::optional<DebuggerKeySchemes::Action> DebuggerKeySchemes::GetActionOfPane (const std::wstring & pane)
{
    for (size_t i = 0; i < std::size (s_kpszPanes); i++)
    {
        if (pane == s_kpszPanes[i])
        {
            return (Action) ((int) Action::ShowDisassembly + (int) i);
        }
    }

    return std::nullopt;
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
//  At a Monitor prompt an empty line and a space are commands themselves --
//  both continue examining -- so there the box keeps them even when empty.
//
////////////////////////////////////////////////////////////////////////////////

bool DebuggerKeySchemes::DoesBoxKeepKey (WPARAM vk, bool ctrl, bool alt, bool boxFocused, bool boxEmpty, bool isEmptyLineInput)
{
    bool  isFunctionKey = vk >= VK_F1 && vk <= VK_F24;
    bool  keeps         = true;



    if (!boxFocused || ctrl || alt || isFunctionKey)
    {
        keeps = false;
    }
    else if (vk == VK_SPACE || vk == VK_RETURN)
    {
        keeps = !boxEmpty || isEmptyLineInput;
    }

    return keeps;
}
