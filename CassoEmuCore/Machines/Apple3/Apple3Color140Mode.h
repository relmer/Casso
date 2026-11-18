#pragma once

#include "Pch.h"

#include "IApple3VideoMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Color140Mode
//
//  140 by 192 in 16 colors: each line's bytes alternate between buffer 1
//  and buffer 2, or on page 2 buffers 3 and 4. Each 4 bytes are 28 bits,
//  7 pixels of 4 bits, least significant first, each 4 pixels wide.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Color140Mode : public IApple3VideoMode
{
public:
    void          Decode      (const Apple3VideoInput & input, Apple3Screen & screen) const override;
    const char  * GetModeName () const override { return "apple3-color140"; }
};