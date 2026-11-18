#pragma once

#include "Pch.h"

#include "IApple3VideoMode.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3TextMode
//
//  40-column black and white text: 40 by 24 characters from page 1 at
//  $0400-$07FF or page 2 at $0800-$0BFF, with inverse and flashing.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3TextMode : public IApple3VideoMode
{
public:
    void          Decode      (const Apple3VideoInput & input, Apple3Screen & screen) const override;
    const char  * GetModeName () const override { return "apple3-text40"; }
};
