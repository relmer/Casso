#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/AppleKeyboard.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AppleKeyMapping
//
//  Host virtual-key codes to the keys an Apple keyboard has.
//
//  These are classifiers over a WPARAM and nothing else -- no window, no
//  keyboard state, no machine. They lived as private helpers on the shell,
//  where the only way to check that Escape still reaches the guest was to press
//  Escape and look, and a mapping that quietly stopped working would show up as
//  a key that does nothing rather than as a failure.
//
//  Which keys a given model physically HAS is a separate question and not one
//  this class answers: a ][+ has no TAB key, yet Ctrl+I on that keyboard still
//  sends $09, so the mapping is the same and the machine decides what it does
//  with it.
//
////////////////////////////////////////////////////////////////////////////////

class AppleKeyMapping
{
public:

    //  A key the Apple keyboard identifies by the key itself rather than by
    //  the character it sends. False when the host key is not one of them.
    static bool  TryMapVkToSpecialKey (WPARAM vk, AppleSpecialKey & outKey);

    //  Whether Windows also manufactures a WM_CHAR for this key, which then
    //  has to be swallowed so the key is not delivered twice -- or, on a
    //  machine that refused the key, delivered after all.
    static bool  DoesSpecialKeySynthesizeChar (AppleSpecialKey key);

    //  The four cursor keys, which double as the joystick when the user has
    //  asked for that.
    static bool  IsArrowVk (WPARAM vk);
};
