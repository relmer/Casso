#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IW5100Backend
//
//  What is behind the W5100's sockets. The chip handles its registers and
//  buffers itself and calls this for everything that involves a network:
//  each socket command after the chip has applied its own part of it, and a
//  software or hardware reset, after which no socket may stay open.
//
////////////////////////////////////////////////////////////////////////////////

class IW5100Backend
{
public:

    virtual ~IW5100Backend () = default;

    virtual void  OnCommand (int socket, Byte command) = 0;
    virtual void  OnReset   ()                         = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NullW5100Backend
//
//  A chip with no network behind it: commands change only what the chip
//  changes itself.
//
////////////////////////////////////////////////////////////////////////////////

class NullW5100Backend : public IW5100Backend
{
public:

    void  OnCommand (int socket, Byte command) override { (void) socket; (void) command; }
    void  OnReset   ()                         override {}
};
