#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Floppy35Op
//
////////////////////////////////////////////////////////////////////////////////

enum class Floppy35Op : uint8_t
{
    MotorOn,
    MotorOff,
    Step,
    Side,
    Eject,
    Insert,
    AddressMark,
    DataRead,
    DataWrite,
};





////////////////////////////////////////////////////////////////////////////////
//
//  Floppy35Event
//
//  One operation of a 3.5-inch mechanism: the drive on its controller, the
//  track and side under the head, and the sector for a read or write.
//
////////////////////////////////////////////////////////////////////////////////

struct Floppy35Event
{
    uint64_t    cycle  = 0;
    Byte        drive  = 0;
    Floppy35Op  op     = Floppy35Op::MotorOn;
    Byte        track  = 0;
    Byte        side   = 0;
    Byte        sector = 0;
};
