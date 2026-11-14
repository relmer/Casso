#pragma once

#include "Pch.h"

#include "Machines/Apple2/Apple2c/Apple2c.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2cPlus
//
//  The Apple //c Plus of 1988: a //c with ROM 05, an internal Apple 3.5 Drive
//  in place of the 5.25-inch drive, the MIG chip that switches the drive and
//  has RAM for the accelerator's settings, and a 4 MHz accelerator compatible
//  with the Zip Chip.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2cPlus : public Apple2c
{
public:
    std::string  GetId              () const override { return ("Apple2cPlus"); }
    std::string  GetCpuManufacturer () const override { return ("VLSI"); }

    //  Its built-in drive is the 3.5-inch drive, not a Disk II mechanism.
    bool  HasBuiltInDrive () const override { return (false); }

    uint16_t  GetWozHardwareFlag () const override { return (WozCompatibility::kApple2cPlus); }
};
