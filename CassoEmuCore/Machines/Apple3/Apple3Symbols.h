#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Symbols
//
//  The Apple ///'s symbol table, as Casso debug file text: the I/O space at
//  $C000-$C0FF, the D and E VIA registers at $FFD0-$FFEF, and the
//  environment, zero page and bank registers among them. The addresses are
//  those of the Apple /// Level 2 Service Reference Manual and the SOS
//  Device Driver Writer's Guide, listed in the spec's hardware map.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Symbols
{
public:
    static const char *  GetMain ();
};
