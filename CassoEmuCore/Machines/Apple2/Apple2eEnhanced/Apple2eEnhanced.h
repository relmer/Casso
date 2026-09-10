#pragma once

#include "Pch.h"

#include "Machines/Apple2/Apple2e/Apple2e.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2eEnhanced
//
//  The Enhanced //e of 1985: a //e with a 65C02 and new ROMs.
//
//  The enhancement was the processor, and this class says so by overriding
//  exactly that. It is a sibling of the //c rather than its parent: the //c
//  shipped a year earlier and brought the 65C02 first, and the Enhanced //e was
//  Apple bringing that back to the //e, so deriving either from the other would
//  invert what happened.
//
//  Their entire shared surface is the CPU. No intermediate base is introduced
//  to hold one value, because a class whose whole content is one assignment
//  costs a reader more than the duplicated line does.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2eEnhanced : public Apple2e
{
public:
    std::string  GetId             () const override { return ("Apple2eEnhanced"); }
    std::string  GetCpu            () const override { return ("65C02"); }
    std::string  GetCpuManufacturer() const override { return ("Rockwell"); }
};
