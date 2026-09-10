#pragma once

#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2e.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2c
//
//  The Apple //c of 1984: an //e with a 65C02 and everything soldered in.
//
//  It runs the //e family's devices unchanged, which is why their type names
//  say //e and not //c -- the //e is where they debuted, not the only machine
//  that has them.
//
//  Its slot count is ZERO, and that is a count rather than a facet removed
//  from it. A //c genuinely has no slots: its expansion is a row of back-panel
//  ports, and what is plugged into those is the owner's business and lives in
//  its JSON. Every loop that walks slots keeps working against a count of
//  none, so nothing had to be taken away for this to be true.
//
//  Derives from the //e rather than from the Enhanced //e, which shipped a year
//  later.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2c : public Apple2e
{
public:
    std::string  GetId             () const override { return ("Apple2c"); }
    std::string  GetCpu            () const override { return ("65C02"); }
    std::string  GetCpuManufacturer() const override { return ("Rockwell"); }

    int  GetSlotCount () const override { return (0); }
};
