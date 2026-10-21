#pragma once

#include "Pch.h"

#include "Core/IInterruptController.h"
#include "Core/MemoryDevice.h"
#include "Devices/Network/VirtualNetwork.h"
#include "Devices/Network/W5100.h"
#include "Devices/Network/W5100SocketBridge.h"

class IHostNetwork;
class MemoryBus;
class NetworkCardSettings;

struct DeviceConfig;





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard
//
//  The a2RetroSystems Uthernet II: a W5100 in indirect bus mode behind
//  $C0n4-$C0n7 (n = slot + 8), with no ROM. $C0n0-$C0n3 are not decoded.
//
//  Host networking runs on the emulation thread: `Tick` counts CPU cycles
//  and, once every kPumpCycles, polls the host sockets if any socket or flow
//  is open and updates the interrupt line. Between those a tick is an add
//  and a compare. A register write updates the interrupt line at once, so a
//  handler that clears the chip's interrupt does not see it again.
//
//  The card is not connected to the Apple II RESET line, so Ctrl-Reset has no
//  effect on the chip; power-on resets it.
//
////////////////////////////////////////////////////////////////////////////////

class UthernetCard : public MemoryDevice
{
public:

    static constexpr Word      kSlotIoBase     = 0xC080;
    static constexpr Word      kSlotIoStride   = 0x10;
    static constexpr Word      kRegisterOffset = 0x4;
    static constexpr Word      kRegisterCount  = 4;
    static constexpr uint32_t  kPumpCycles     = 1023;
    static constexpr int       kDefaultSlot    = 3;

    explicit UthernetCard (int slot);
    ~UthernetCard () override;

    static unique_ptr<MemoryDevice>  Create (const DeviceConfig & config, MemoryBus & bus);

    Byte  Read       (Word address)              override;
    void  Write      (Word address, Byte value)  override;
    Word  GetStart   () const                    override { return m_ioStart; }
    Word  GetEnd     () const                    override { return m_ioEnd; }
    void  Reset      ()                          override;
    void  SoftReset  ()                          override;
    void  PowerCycle (Prng & prng)               override;

    HRESULT  AttachInterruptController (IInterruptController * ic);
    void     AttachNetwork             (IHostNetwork * network, const NetworkCardSettings & settings);
    void     ApplySettings             (const NetworkCardSettings & settings);

    // Port forwards whose host port could not be bound, as sentences.
    const std::vector<std::string> & GetNetworkErrors () const { return m_network.GetErrors(); }

    void  Tick (uint32_t cycles)
    {
        m_cyclesSincePump += cycles;

        if (m_cyclesSincePump >= kPumpCycles)
        {
            OnPumpInterval();
        }
    }

    int                  GetSlot   () const { return m_slot; }
    W5100              & GetChip   ()       { return m_chip; }
    W5100SocketBridge  & GetBridge ()       { return m_bridge; }
    VirtualNetwork     & GetNetwork()       { return m_network; }
    uint64_t             GetCycles () const { return m_cycles; }

private:

    void  OnPumpInterval ();
    void  UpdateIrq      ();

    int                      m_slot            = 0;
    Word                     m_ioStart         = 0;
    Word                     m_ioEnd           = 0;
    W5100                    m_chip;
    VirtualNetwork           m_network;
    W5100SocketBridge        m_bridge;
    IInterruptController   * m_ic              = nullptr;
    IrqSourceId              m_irqSource       = 0;
    bool                     m_irqBound        = false;
    bool                     m_irqAsserted     = false;
    uint32_t                 m_cyclesSincePump = 0;
    uint64_t                 m_cycles          = 0;
};
