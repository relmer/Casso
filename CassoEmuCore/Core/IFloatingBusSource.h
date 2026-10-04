#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IFloatingBusSource
//
//  What a read that no device drives returns. On an Apple II the data bus is
//  not left to hold its last value: the video scanner fetches a byte from
//  RAM on every cycle, and a read that drives nothing sees that byte. A
//  machine that models the scanner gives the bus one of these; without one
//  the bus falls back to the last value any device drove.
//
////////////////////////////////////////////////////////////////////////////////

class IFloatingBusSource
{
public:
    virtual         ~IFloatingBusSource() = default;

    virtual Byte     GetFloatingBusByte() = 0;
};
