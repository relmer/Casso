#pragma once

#include "Pch.h"

#include "IApple3VideoMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple380ColTextMode
//
//  80-column black and white text. Page 1 has the first, third, fifth...
//  characters of each row at $0400 and the second, fourth, sixth... at
//  $0800; page 2 swaps the two.
//
////////////////////////////////////////////////////////////////////////////////

class Apple380ColTextMode : public IApple3VideoMode
{
public:
    void          Decode      (const Apple3VideoInput & input, Apple3Screen & screen) const override;
    const char  * GetModeName () const override { return "apple3-text80"; }
};
