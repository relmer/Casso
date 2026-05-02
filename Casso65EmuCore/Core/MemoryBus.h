#pragma once

#include "Pch.h"
#include "MemoryDevice.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BusEntry
//
////////////////////////////////////////////////////////////////////////////////

struct BusEntry
{
    Word            start;
    Word            end;
    MemoryDevice *  device;
};





////////////////////////////////////////////////////////////////////////////////
//
//  MemoryBus
//
////////////////////////////////////////////////////////////////////////////////

class MemoryBus
{
public:
    MemoryBus ();

    Byte ReadByte     (Word address);
    void WriteByte    (Word address, Byte value);

    void AddDevice    (MemoryDevice * device);
    void RemoveDevice (MemoryDevice * device);

    HRESULT Validate () const;

    void Reset ();

    const vector<BusEntry> & GetEntries () const { return m_entries; }

private:
    MemoryDevice * FindDevice (Word address) const;

    vector<BusEntry>   m_entries;
    Byte                    m_floatingBusValue = 0xFF;
};
