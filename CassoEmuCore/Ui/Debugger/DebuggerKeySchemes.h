#pragma once

#include "Core/DxuiKeyMap.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeyScheme
//
////////////////////////////////////////////////////////////////////////////////

enum class DebuggerKeyScheme
{
    VisualStudio,
    AppleWin,
    GSSquared,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DebuggerKeySchemes
//
//  The debugger window's three keyboard schemes as DxuiKeyMaps, chosen in
//  preferences and independent of the command mode.
//
//  AppleWin's help names Space, Ctrl+Space, Shift+Space and Ctrl+Down;
//  GSSquared's names Space, Return, O and R. Neither names a key to pause or to
//  toggle a breakpoint, so both borrow Visual Studio's Shift+F5 and F9 rather
//  than leave those actions without a key.
//
////////////////////////////////////////////////////////////////////////////////

class DebuggerKeySchemes
{
public:
    enum class Action
    {
        Run = 1,
        Pause,
        StepInto,
        StepOver,
        StepOut,
        ToggleBreakpoint,
        RunToCursor,

        First = Run,
        Last  = RunToCursor,
    };

    static constexpr DebuggerKeyScheme  kDefault = DebuggerKeyScheme::VisualStudio;

    static const DxuiKeyMap &  GetMap   (DebuggerKeyScheme scheme);
    static std::string         GetName  (DebuggerKeyScheme scheme);
    static bool                TryParse (const std::string & name, DebuggerKeyScheme & scheme);

    //  Whether a focused text box keeps a key rather than the scheme getting it.
    //  Function keys and Ctrl or Alt chords always reach the scheme; Space and
    //  Enter reach it only from an empty box; every other key stays with the box.
    static bool  DoesBoxKeepKey (WPARAM vk, bool ctrl, bool alt, bool boxFocused, bool boxEmpty);
};
