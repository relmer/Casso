#pragma once

#include "Pch.h"
#include "Core/MemoryDevice.h"
#include "Core/MachineConfig.h"
#include "Core/MemoryBus.h"

class ITapeDeckPort;





////////////////////////////////////////////////////////////////////////////////
//
//  CassettePort
//
//  The motherboard cassette interface of the ][, ][+ and //e: two jacks on the
//  back panel. Any access to $C020-$C02F toggles the output flip-flop that
//  drives the cassette-out jack. Bit 7 of $C060 (and its mirror $C068) is the
//  comparator on the cassette-in jack; the game port (][, ][+) or the keyboard
//  (//e) decodes those addresses and reads the level from this device.
//
//  The port passes signal only. A device plugged in receives edges at the bus
//  cycle they happen, and the guest program's code interprets them.
//
////////////////////////////////////////////////////////////////////////////////

class CassettePort : public MemoryDevice
{
public:
    explicit CassettePort (MemoryBus * bus);

    Byte Read      (Word address) override;
    void Write     (Word address, Byte value) override;
    Word GetStart  () const override { return kFirstOutputAddress; }
    Word GetEnd    () const override { return kLastOutputAddress; }
    void Reset     () override;
    void SoftReset () override;

    void  SetCpuCycleSource (const uint64_t * source) { m_cycleSource = source; }
    void  SetDeck           (ITapeDeckPort * deck)    { m_deck = deck; }

    void  ToggleOutput             ();
    bool  ReadInputLevel           ();
    Byte  ReadInputOverFloatingBus ();
    bool  GetOutputLevel           () const { return m_outputLevel; }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

    static constexpr Word  kFirstOutputAddress = 0xC020;
    static constexpr Word  kLastOutputAddress  = 0xC02F;
    static constexpr Word  kInputAddress       = 0xC060;
    static constexpr Word  kInputMirrorAddress = 0xC068;
    static constexpr Byte  kInputBit           = 0x80;

private:
    uint64_t  GetCycle () const;

    MemoryBus        * m_bus         = nullptr;
    const uint64_t   * m_cycleSource = nullptr;
    ITapeDeckPort    * m_deck        = nullptr;
    bool               m_outputLevel = false;
};
