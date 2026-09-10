#pragma once

#include "Pch.h"

#include "Machines/IMachine.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2
//
//  The Apple ][ of 1977, and the base of the whole line: 48K on one bank, an
//  uppercase-only keyboard, the original soft switches, a game port, and the
//  three video modes the machine shipped with.
//
//  There is no abstract Apple class above this one. The ][ is not an instance
//  of some more general Apple II -- it IS the general Apple II, and every later
//  model is a ][ with things added or replaced. Rooting the chain anywhere else
//  would mean inventing a machine nobody built.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2 : public IMachine
{
public:
    std::string  GetId             () const override { return ("Apple2"); }
    std::string  GetCpu            () const override { return ("6502"); }
    std::string  GetCpuManufacturer() const override { return ("MOS Technology"); }
    std::string  GetKeyboardLayout () const override { return ("apple2-family-layout"); }

    std::vector<RamRegion>       GetRam             () const override;
    std::vector<InternalDevice>  GetInternalDevices () const override;
    std::vector<std::string>     GetVideoModes      () const override;

    int   GetSlotCount       () const override { return (7); }
    bool  HasGamePortDevice  () const override { return (true); }
};
