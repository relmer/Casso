#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IHostCapsLock
//
//  The host keyboard's Caps Lock toggle: whether it is on, a way to flip it,
//  and a way to tell a flip this process made from one the user made.
//
//  Flipping is a keystroke rather than a state write because the LED follows
//  the input stream, not the key-state table. A synthesized press arrives back
//  at the window as an ordinary VK_CAPITAL key-up, so the key handler asks
//  WasLastKeySynthesized before it reads that key-up as the user's toggle.
//
////////////////////////////////////////////////////////////////////////////////

class IHostCapsLock
{
public:

    virtual ~IHostCapsLock () = default;

    virtual bool  IsOn                   () const = 0;
    virtual void  Toggle                 ()       = 0;
    virtual bool  WasLastKeySynthesized  () const = 0;
};
