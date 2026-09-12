#pragma once

#include "Pch.h"

#include "Ui/UiCommandTypes.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellKeyOwner
//
//  Who a keydown belongs to. Guest means the emulated machine; every other
//  value names a part of the chrome that consumes the key instead.
//
////////////////////////////////////////////////////////////////////////////////

enum class ShellKeyOwner
{
    Guest,
    PaddleExit,     // Esc, which returns the pointer mapping to Off
    Toolbar,        // an open picker, or a flyout opened by keyboard
    Chrome          // the keyboard-focus ring, or an open menu
};





////////////////////////////////////////////////////////////////////////////////
//
//  ShellKeyRouting
//
//  Decides which part of the shell owns a keydown, over chrome state and a
//  WPARAM and nothing else -- no window, no machine, no message pump.
//
//  One physical press produces TWO messages. Windows manufactures a WM_CHAR
//  from a WM_KEYDOWN whether or not anything consumed the keydown, so a key
//  the chrome took arrives a second time as a character. Both have to be
//  routed the same way, and while the decision was made separately in each
//  handler the two drifted: Esc leaving paddle mode went to the chrome as a
//  key and to the //e as a character.
//
//  Recomputing the state in the character handler is not a fix, because the
//  key frequently CHANGES the state it would be re-tested against -- Escape
//  over an open picker closes the picker, so by the time the character arrives
//  nothing owns the keyboard any more. The decision is therefore made once, on
//  the keydown, and the character handler reuses the result.
//
//  The host-meta shortcuts (Alt mnemonics, F10, Ctrl+V) are deliberately NOT
//  here. That ladder both tests and acts -- it opens the menu as it goes -- so
//  it cannot be a classifier, and it already records its own result.
//
////////////////////////////////////////////////////////////////////////////////

class ShellKeyRouting
{
public:

    //  The chrome state the verdict is reached over. Every field is a plain
    //  value the shell already holds; nothing here reaches back into it.
    struct State
    {
        InputMappingMode  pointerMode         = InputMappingMode::Off;
        bool              toolbarOwnsKeyboard = false;
        bool              isChromeFocused     = false;
        bool              isMenuOpen          = false;
    };

    //  Precedence runs paddle, then toolbar, then chrome, matching the order
    //  the pre-checks run in. It matters: a picker can be open while the
    //  focus ring is also active, and the picker is the one that owns the key.
    static ShellKeyOwner  GetKeyOwner (const State & state, WPARAM vk);

    //  Whether the character Windows makes from this keydown has to be
    //  swallowed. True for everything the shell claimed, so a new claim cannot
    //  arrive without one.
    static bool           DoesOwnerSwallowChar (ShellKeyOwner owner);
};
