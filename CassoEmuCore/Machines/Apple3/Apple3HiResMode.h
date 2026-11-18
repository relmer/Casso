#pragma once

#include "Pch.h"

#include "IApple3VideoMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3HiResMode
//
//  280 by 192 black and white: each dot two pixels wide, from buffer 1 or,
//  on page 2, buffer 2.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3HiResMode : public IApple3VideoMode
{
public:
    void          Decode      (const Apple3VideoInput & input, Apple3Screen & screen) const override;
    const char  * GetModeName () const override { return "apple3-hires"; }
};