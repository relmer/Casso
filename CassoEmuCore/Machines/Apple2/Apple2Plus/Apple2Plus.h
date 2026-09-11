#pragma once

#include "Pch.h"

#include "Machines/Apple2/Apple2/Apple2.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2Plus
//
//  The Apple ][ plus of 1979: an Apple ][ with the Autostart ROM and Applesoft
//  in place of Integer BASIC.
//
//  It overrides nothing but its own name. Both of its differences from the ][
//  are a ROM file and a card in a slot, and both of those are the owner's to
//  set, so they live in its JSON. An almost-empty class is the correct answer
//  here rather than a sign the hierarchy is wrong: this really is a ][ with
//  different ROMs in it.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2Plus : public Apple2
{
public:
    std::string  GetId () const override { return ("Apple2Plus"); }
};
