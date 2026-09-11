#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IMachine
//
//  What a machine IS, as opposed to how its owner set it up.
//
//  Nobody chose the //e's keyboard or the ][+'s soft switches; those are the
//  machine. They are answered here, in code, because a value nobody may change
//  has no business in a file anyone may edit.
//
//  What an owner COULD change on the physical machine stays in the machine's
//  JSON: what is in each slot, what is on each port, which ROM to load. The
//  test is whether it could have been done with a screwdriver.
//
//  The Apple II line implements this as a chain rooted in the order the
//  machines were built -- the ][ is the base, and every later model is a ][
//  with things added or replaced. A Commodore or Atari family would implement
//  it separately, which is the whole reason this interface exists above the
//  Apple ][ rather than the ][ being the only root.
//
////////////////////////////////////////////////////////////////////////////////

class IMachine
{
public:
    virtual ~IMachine () = default;

    //  The model's directory name under Resources/Machines, e.g. "Apple2e".
    virtual std::string  GetId () const = 0;

    virtual std::string  GetCpu             () const = 0;
    virtual std::string  GetCpuManufacturer () const = 0;
    virtual std::string  GetKeyboardLayout  () const = 0;

    virtual std::vector<RamRegion>       GetRam             () const = 0;
    virtual std::vector<InternalDevice>  GetInternalDevices () const = 0;
    virtual std::vector<std::string>     GetVideoModes      () const = 0;

    //  How many card slots the machine has. Zero is a real answer -- a //c has
    //  none, and that is a count rather than a facet taken away from it.
    virtual int  GetSlotCount () const = 0;

    //  Whether the machine builds a game-port device of its own. False on the
    //  //e and later, where the soft-switch bank absorbed the paddle timer and
    //  PREAD, so a derived machine declines a device its parent creates rather
    //  than having one removed from it after the fact.
    virtual bool  HasGamePortDevice () const = 0;

    //  Whether the machine's case carries switches the user can reach -- the
    //  //c's 40/80 column and keyboard-layout switches. A presentation
    //  question rather than an emulation one, and the machine is who knows the
    //  answer.
    virtual bool  HasCaseSwitches () const = 0;

    //  Whether the machine's drive is built into it rather than plugged into a
    //  card. Decides which drive the desk scene draws.
    virtual bool  HasBuiltInDrive () const = 0;
};
