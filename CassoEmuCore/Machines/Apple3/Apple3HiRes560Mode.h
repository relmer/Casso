#pragma once

#include "Pch.h"

#include "IApple3VideoMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3HiRes560Mode
//
//  560 by 192 black and white: each line's bytes alternate between buffer
//  1 and buffer 2, or on page 2 buffers 3 and 4, a pixel per dot.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3HiRes560Mode : public IApple3VideoMode
{
public:
    void          Decode      (const Apple3VideoInput & input, Apple3Screen & screen) const override;
    const char  * GetModeName () const override { return "apple3-hires560"; }
};