#include "Pch.h"

#include "Shell/Input/ShellKeyRouting.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellKeyRouting::GetKeyOwner
//
//  The ladder, in the order the shell's pre-checks run it.
//
//  The rewind key leads: F2 is never the guest's, whatever the chrome is
//  doing. Paddle is next because Esc is the documented way out of a captured
//  pointer and has to work whatever the chrome is doing. Those two are the
//  arms that turn on WHICH key was pressed; the rest take every key while
//  they are up, so a letter typed at a focused menu title does not go to the
//  //e.
//
////////////////////////////////////////////////////////////////////////////////

ShellKeyOwner ShellKeyRouting::GetKeyOwner (
    const State  & state,
    WPARAM         vk)
{
    ShellKeyOwner  owner = ShellKeyOwner::Guest;



    if (vk == VK_F2)
    {
        owner = ShellKeyOwner::Rewind;
    }
    else if (state.pointerMode == InputMappingMode::Paddle && vk == VK_ESCAPE)
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
