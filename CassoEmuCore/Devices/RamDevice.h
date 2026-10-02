#pragma once

#include "Pch.h"
#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  RamDevice
//
////////////////////////////////////////////////////////////////////////////////

class RamDevice : public MemoryDevice, public IMachineState
{
public:
    RamDevice (Word start, Word end);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return m_start; }
    Word GetEnd   () const override { return m_end; }
    void Reset    () override;

    void SoftReset  () override;
    void PowerCycle (Prng & prng) override;

    Byte * GetData () { return m_data.data (); }

    // IMachineState: every byte. The address range is wiring, saved only to
    // check that the RAM loading the state covers the same addresses.
    HRESULT SaveState (StateWriter & writer) const override;
    HRESULT LoadState (StateReader & reader) override;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('R', 'A', 'M', ' ');
    static constexpr uint16_t  kStateVersion = 1;

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    Word          m_start;
    Word          m_end;
    vector<Byte>  m_data;
};
