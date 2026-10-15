#pragma once

#include "Pch.h"





////////////////////////////////////////////////////////////////////////////////
//
//  IIoAccessSink
//
//  A listener on the bus's I/O space, $C000-$CFFF: each read or write a
//  device there decodes at an address the listener's IoAccessFilter
//  includes, after the device has acted on it, on the thread that runs the
//  machine. An address no device decodes is not reported.
//
//  The bus tests for a listener on each access a device decodes, and does
//  nothing more when there is none; with one, it calls the listener when
//  the listener's filter includes the address.
//
////////////////////////////////////////////////////////////////////////////////

class IIoAccessSink
{
public:
    virtual       ~IIoAccessSink() = default;

    virtual void  OnIoAccess (Word address, Byte value, bool isWrite) = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  IoAccessFilter
//
//  The I/O addresses a listener takes reads and writes of, one bit per
//  address of $C000-$CFFF each.
//
////////////////////////////////////////////////////////////////////////////////

struct IoAccessFilter
{
    static constexpr Word    kFirst = 0xC000;
    static constexpr size_t  kSize  = 0x1000;

    std::bitset<kSize>  reads;
    std::bitset<kSize>  writes;

    void  AddReads  (Word first, Word last) { for (Word a = first; a <= last; a++) { reads.set  (a - kFirst); } }
    void  AddWrites (Word first, Word last) { for (Word a = first; a <= last; a++) { writes.set (a - kFirst); } }
};
