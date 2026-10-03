#pragma once





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
//  Neither has a find of its own either, so all three schemes find with the
//  Windows keys: Ctrl+F to open the find bar, F3 and Shift+F3 for the next
//  and previous match. No scheme binds any of them to anything else.
//
//  None of the three has a key to run one video frame; all take F6, which
//  none of them binds.
//
//  Every scheme steps back with Alt and the forward step's Visual Studio key:
//  Alt+F11 into, Alt+F10 over, Alt+Shift+F11 out. The Visual Studio scheme
//  also takes Visual Studio's own chords, Ctrl+R then the forward step's key,
//  and shows those in its menus.
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
        Find,
        FindNext,
        FindPrevious,
        RunFrame,
        StepBackInto,
        StepBackOver,
        StepBackOut,

        First = Run,
        Last  = StepBackOut,
    };

    static constexpr DebuggerKeyScheme  kDefault = DebuggerKeyScheme::VisualStudio;

    static const DxuiKeyMap &  GetMap   (DebuggerKeyScheme scheme);
    static std::string         GetName  (DebuggerKeyScheme scheme);
    static bool                TryParse (const std::string & name, DebuggerKeyScheme & scheme);

    //  Whether a focused text box keeps a key rather than the scheme getting it.
    //  Function keys and Ctrl or Alt chords always reach the scheme; Space and
    //  Enter reach it only from an empty box, and not even then when an empty
    //  line is itself input, as it is at a Monitor prompt; every other key stays
    //  with the box.
    static bool  DoesBoxKeepKey (WPARAM vk, bool ctrl, bool alt, bool boxFocused, bool boxEmpty, bool isEmptyLineInput);
};
