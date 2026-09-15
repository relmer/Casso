#pragma once





////////////////////////////////////////////////////////////////////////////////
//
//  BusAccess
//
////////////////////////////////////////////////////////////////////////////////

enum class BusAccess
{
    Read,
    Write,
};





////////////////////////////////////////////////////////////////////////////////
//
//  IWatchSink
//
//  Receives every read and write the bus makes to a watched page, with the
//  value read or written. A write to a memory-backed page also carries the
//  byte it replaced; a write to a device, or any read, carries none. Pages are
//  watched whole, so the sink decides which addresses matter.
//
////////////////////////////////////////////////////////////////////////////////

class IWatchSink
{
public:
    virtual ~IWatchSink() = default;

    virtual void  OnWatchedAccess (Word                  address,
                                   Byte                  value,
                                   BusAccess             access,
                                   std::optional<Byte>   previous) = 0;
};
