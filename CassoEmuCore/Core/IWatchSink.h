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
//  The CPU's own access sink is also told just before each write the CPU
//  makes, while the byte it replaces can still be read, and of a power
//  cycle, which refills RAM; the bus tells its sinks neither.
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

    virtual void  OnBeforeWrite   (Word address) { (void) address; }
    virtual void  OnPowerCycle    ()             {}
};
