#pragma once

#include "Pch.h"

#include "Debugger/IoEventLog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ICommEventSink
//
//  Where a serial port and a network card report what the debugger's serial
//  and network logs list, on the thread that runs the machine: a byte sent
//  or received, a change of the modem lines, the settings or a socket's
//  state, and data a socket sent or received with its bytes. The receiver
//  stamps each event with the machine's position and cycle.
//
////////////////////////////////////////////////////////////////////////////////

class ICommEventSink
{
public:
    virtual       ~ICommEventSink() = default;

    virtual void  OnCommEvent (const IoEvent & event, std::span<const Byte> bytes) = 0;
};
