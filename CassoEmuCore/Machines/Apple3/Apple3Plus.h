#pragma once

#include "Pch.h"

#include "Machines/Apple3/Apple3.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Plus
//
//  The Apple /// Plus of 1983: an Apple /// with 256K and the clock/calendar
//  as standard, and a text interlace switch on the left side of the case.
//  Its keyboard has the Apple ///'s keys and codes (Apple /// Plus
//  Owner's Guide, chapter 2 and appendix D).
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Plus : public Apple3
{
public:
    std::string               GetId            () const override { return ("Apple3Plus"); }
    std::vector<std::string>  GetMemoryOptions () const override { return { "256K" }; }
};
