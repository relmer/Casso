#pragma once

#include "Pch.h"

class Apple3Screen;
class IGlyphSource;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3VideoInput
//
//  What a mode decodes from: the system bank, the glyphs, the page switch,
//  the flash phase, bank 0 at $2000-$9FFF for the graphics modes, and the
//  smooth scroll offset, from 0 to 7 scan lines within each group of eight.
//
////////////////////////////////////////////////////////////////////////////////

struct Apple3VideoInput
{
    const std::function<Byte (Word)>  & readSystem;
    const IGlyphSource                & glyphs;
    bool                                isPage2   = false;
    bool                                isFlashOn = false;
    const std::function<Byte (Word)>  * readGraphics = nullptr;
    int                                 scrollOffset = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IApple3VideoMode
//
//  An Apple /// display mode: decodes video memory into color numbers.
//
////////////////////////////////////////////////////////////////////////////////

class IApple3VideoMode
{
public:
    virtual                ~IApple3VideoMode() = default;

    virtual void            Decode      (const Apple3VideoInput & input, Apple3Screen & screen) const = 0;
    virtual const char    * GetModeName () const = 0;
};
