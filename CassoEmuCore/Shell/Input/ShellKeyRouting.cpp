#include "Pch.h"

#include "Shell/Input/ShellKeyRouting.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellKeyRouting::GetKeyOwner
//
//  The ladder, in the order the shell's pre-checks run it.
//
//  Paddle leads because Esc is the documented way out of a captured pointer
//  and has to work whatever the chrome is doing. It is the only arm that turns
//  on WHICH key was pressed; the rest claim every key while they are up, which
//  is what keeps a letter typed at a focused menu title out of the //e.
//
////////////////////////////////////////////////////////////////////////////////

ShellKeyOwner ShellKeyRouting::GetKeyOwner (
    const State  & state,
    WPARAM         vk)
{
    ShellKeyOwner  owner = ShellKeyOwner::Guest;



    if (state.pointerMode == InputMappingMode::Paddle && vk == VK_ESCAPE)
    {
        owner = ShellKeyOwner::PaddleExit;
    }
    else if (state.toolbarOwnsKeyboard)
    {
        owner = ShellKeyOwner::Toolbar;
    }
    else if (state.isChromeFocused || state.isMenuOpen)
    {
        owner = ShellKeyOwner::Chrome;
    }

    return owner;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellKeyRouting::DoesOwnerSwallowChar
//
//  Anything the shell claimed swallows its character. Stated as a function of
//  the owner rather than left to each arm, so a claim added later cannot
//  forget: the swallow follows from not being the guest's.
//
////////////////////////////////////////////////////////////////////////////////

bool ShellKeyRouting::DoesOwnerSwallowChar (ShellKeyOwner owner)
{
    bool  swallows = owner != ShellKeyOwner::Guest;



    return (swallows);
}
