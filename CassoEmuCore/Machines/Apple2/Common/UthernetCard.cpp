#include "Pch.h"

#include "Machines/Apple2/Common/UthernetCard.h"
#include "Config/NetworkCardSettings.h"
#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::UthernetCard
//
//  Slot n's four registers are $C084 + n * $10 through $C087 + n * $10.
//
////////////////////////////////////////////////////////////////////////////////

UthernetCard::UthernetCard (int slot)
    : m_bridge (m_chip, m_network)
{
    m_slot    = slot;
    m_ioStart = (Word) (kSlotIoBase + slot * kSlotIoStride + kRegisterOffset);
    m_ioEnd   = (Word) (m_ioStart + kRegisterCount - 1);

    m_chip.SetBackend (&m_bridge);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::~UthernetCard
//
//  The chip's backend goes before the chip does.
//
////////////////////////////////////////////////////////////////////////////////

UthernetCard::~UthernetCard()
{
    m_chip.SetBackend (nullptr);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::Create
//
//  ComponentRegistry factory; slot 3 when the configuration has none.
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> UthernetCard::Create (
    const DeviceConfig  & config,
    MemoryBus           & bus)
{
    int  slot = config.hasSlot ? config.slot : kDefaultSlot;



    (void) bus;

    return make_unique<UthernetCard> (slot);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::Read
//
////////////////////////////////////////////////////////////////////////////////

Byte UthernetCard::Read (Word address)
{
    return m_chip.ReadIndirect (address - m_ioStart);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::Write
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::Write (
    Word  address,
    Byte  value)
{
    m_chip.WriteIndirect (address - m_ioStart, value);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::Reset
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::Reset()
{
    m_chip.HardwareReset();
    m_cyclesSincePump = 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::SoftReset
//
//  Ctrl-Reset has no effect on the chip. The machine clears every interrupt
//  source after resetting the bus, so the next Tick asserts the line again
//  while the chip has an unmasked interrupt pending.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::SoftReset()
{
    m_irqAsserted = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::PowerCycle
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::PowerCycle (Prng & prng)
{
    (void) prng;

    Reset();
    m_irqAsserted = false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::AttachInterruptController
//
////////////////////////////////////////////////////////////////////////////////

HRESULT UthernetCard::AttachInterruptController (IInterruptController * ic)
{
    HRESULT  hr = S_OK;



    CBRAEx (ic, E_INVALIDARG);

    hr = ic->RegisterSource (m_irqSource);
    CHR (hr);

    m_ic       = ic;
    m_irqBound = true;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::AttachNetwork
//
//  With a null network the card has no PC connectivity: connects time out
//  and datagrams are dropped, but DHCP and the virtual gateway work.
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::AttachNetwork (
    IHostNetwork               * network,
    const NetworkCardSettings  & settings)
{
    m_bridge.SetHostNetwork (network);
    m_bridge.SetLanAccess (settings.lanAccess);

    m_network.SetHostNetwork (network);
    m_network.Configure (settings.lanAccess, settings.portForwards);
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::Tick
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::Tick (uint32_t cycles)
{
    m_cycles          += cycles;
    m_cyclesSincePump += cycles;

    if (m_cyclesSincePump >= kPumpCycles)
    {
        m_cyclesSincePump = 0;

        if (m_bridge.IsActive())
        {
            m_bridge.Pump (m_cycles);
        }
    }

    UpdateIrq();
}





////////////////////////////////////////////////////////////////////////////////
//
//  UthernetCard::UpdateIrq
//
////////////////////////////////////////////////////////////////////////////////

void UthernetCard::UpdateIrq()
{
    bool  pending = m_chip.IsInterruptPending();



    if (!m_irqBound || pending == m_irqAsserted)
    {
        return;
    }

    if (pending)
    {
        m_ic->Assert (m_irqSource);
    }
    else
    {
        m_ic->Clear (m_irqSource);
    }

    m_irqAsserted = pending;
}
