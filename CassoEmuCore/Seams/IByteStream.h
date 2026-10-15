#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IByteStream
//
//  A two-way stream of bytes between the PC and an Apple II: a COM port, a
//  TCP connection, or, inside one process, a pair of queues. The Apple II
//  link's protocol reads and writes through this alone, so its tests run on
//  the in-process pair and open no port or socket.
//
//  Writes never block for long: a serial or TCP write is buffered by the
//  system. Reads wait up to the timeout given for the first byte, then
//  return what has arrived.
//
////////////////////////////////////////////////////////////////////////////////

class IByteStream
{
public:
    virtual ~IByteStream() = default;

    virtual HRESULT  Write            (const Byte * data, size_t count) = 0;
    virtual HRESULT  Read             (Byte * buffer, size_t capacity, DWORD timeoutMs, size_t & outRead) = 0;
    virtual size_t   GetReadableCount () = 0;
    virtual bool     IsClosed         () const = 0;
    virtual void     Close            () = 0;
};
