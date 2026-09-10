#pragma once

#include "Pch.h"

#include "Machines/Apple2/Apple2Plus/Apple2Plus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Apple2e
//
//  The Apple //e of 1983, and the machine that introduced nearly everything
//  the later models inherit: an auxiliary RAM bank and the MMU that pages it,
//  extended soft switches, a full keyboard with lowercase and modifiers, and
//  80-column and double hi-res video.
//
//  It derives from the ][+ because that is what Apple redesigned. The device
//  list is REPLACED rather than added to, which is unusual for a derived class
//  and is honest here: of the ][+'s four motherboard devices the //e keeps
//  only the speaker, swaps the keyboard and the soft switches for wider ones,
//  drops the game port, and adds the MMU and the language card. Composing that
//  from the base's list would be four edits to produce five entries.
//
//  The game port is the one genuine removal in the family, and it is expressed
//  as declining to create rather than as deleting after the fact:
//  Apple2eSoftSwitchBank absorbed the paddle timer and the PREAD accumulator,
//  so a separate game-port device would be a second owner of $C070.
//
////////////////////////////////////////////////////////////////////////////////

class Apple2e : public Apple2Plus
{
public:
    std::string  GetId            () const override { return ("Apple2e"); }
    std::string  GetKeyboardLayout() const override { return ("apple2e-family-layout"); }

    std::vector<RamRegion>       GetRam             () const override;
    std::vector<InternalDevice>  GetInternalDevices () const override;
    std::vector<std::string>     GetVideoModes      () const override;

    bool  HasGamePortDevice () const override { return (false); }
};
