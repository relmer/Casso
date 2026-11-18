#pragma once

#include "Pch.h"

#include "IApple3VideoMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3ColorHiResMode
//
//  280 by 192 with colors: the dots from one buffer and, at the same offset
//  in the other, a color byte for each 7 dots, bits 4-7 the foreground and
//  bits 0-3 the background. Page 1 has the dots in buffer 1 and the colors
//  in buffer 2; page 2 swaps the two.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3ColorHiResMode : public IApple3VideoMode
{
public:
    void          Decode      (const Apple3VideoInput & input, Apple3Screen & screen) const override;
    const char  * GetModeName () const override { return "apple3-hires-color"; }
};