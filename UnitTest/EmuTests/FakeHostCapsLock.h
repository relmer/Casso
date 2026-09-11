#pragma once

#include "Pch.h"

#include "Seams/IHostCapsLock.h"





////////////////////////////////////////////////////////////////////////////////
//
//  FakeHostCapsLock
//
//  A host Caps Lock toggle that flips when asked and counts the flips. A
//  test sets `lastKeyWasOurs` to play the key-up our own flip produces.
//
////////////////////////////////////////////////////////////////////////////////

class FakeHostCapsLock : public IHostCapsLock
{
public:

    bool  isOn           = false;
    bool  lastKeyWasOurs = false;
    int   toggles        = 0;


    bool  IsOn () const override
    {
        return isOn;
    }


    void  Toggle () override
    {
        isOn = !isOn;
        toggles++;
    }


    bool  WasLastKeySynthesized () const override
    {
        return lastKeyWasOurs;
    }
};
