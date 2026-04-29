#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LanguageCard
//
//  Bank-switching state machine for $D000-$FFFF via soft switches $C080-$C08F.
//
////////////////////////////////////////////////////////////////////////////////

class LanguageCard : public MemoryDevice
{
public:
    LanguageCard (MemoryBus & bus);

    Byte Read     (Word address) override;
    void Write    (Word address, Byte value) override;
    Word GetStart () const override { return 0xC080; }
    Word GetEnd   () const override { return 0xC08F; }
    void Reset    () override;

    // RAM access for the $D000-$FFFF region — called by a helper device
    Byte ReadRam  (Word address);
    void WriteRam (Word address, Byte value);

    bool IsReadRam    () const { return m_readRam; }
    bool IsWriteRam   () const { return m_writeRam; }
    bool IsBank2      () const { return m_bank2Select; }

    static std::unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    void UpdateState (Byte switchAddr);

    MemoryBus &         m_bus;

    std::vector<Byte>   m_ramBank1;     // 4KB bank 1 at $D000-$DFFF
    std::vector<Byte>   m_ramBank2;     // 4KB bank 2 at $D000-$DFFF
    std::vector<Byte>   m_ramMain;      // 8KB at $E000-$FFFF

    bool    m_readRam;
    bool    m_writeRam;
    bool    m_preWrite;
    bool    m_bank2Select;
    Byte    m_lastSwitch;
};
