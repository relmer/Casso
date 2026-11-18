#pragma once

#include "Pch.h"

#include "IApple3VideoMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3ColorTextMode
//
//  40-column color text: the characters in one text page and their colors
//  in the other, byte for byte. Page 1 has the characters at $0400 and the
//  colors at $0800; page 2 swaps the two. A color byte's bits 4-7 are the
//  foreground and bits 0-3 the background.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3ColorTextMode : public IApple3VideoMode
{
public:
    void          Decode      (const Apple3VideoInput & input, Apple3Screen & screen) const override;
    const char  * GetModeName () const override { return "apple3-text40-color"; }
};
