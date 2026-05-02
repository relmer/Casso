#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RamDevice
//
////////////////////////////////////////////////////////////////////////////////

class RamDevice : public MemoryDevice
{
public:
    RamDevice (Word start, Word end);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return m_start; }
    Word GetEnd   () const override { return m_end; }
    void Reset    () override;

    Byte * GetData () { return m_data.data (); }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    Word                m_start;
    Word                m_end;
    vector<Byte>   m_data;
};
