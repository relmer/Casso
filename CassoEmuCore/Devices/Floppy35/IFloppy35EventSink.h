#pragma once

#include "Pch.h"

#include "Devices/Floppy35/Floppy35Event.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IFloppy35EventSink
//
//  Receives each 3.5-inch mechanism operation, on the emulation thread.
//
////////////////////////////////////////////////////////////////////////////////

class IFloppy35EventSink
{
public:
    virtual ~IFloppy35EventSink() = default;

    virtual void OnFloppy35Event (const Floppy35Event & event) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IFloppy35EventSource
//
//  A device that reports 3.5-inch mechanism events to one sink, or to none.
//
////////////////////////////////////////////////////////////////////////////////

class IFloppy35EventSource
{
public:
    virtual ~IFloppy35EventSource() = default;

    virtual void SetFloppy35EventSink (IFloppy35EventSink * sink) = 0;
};
