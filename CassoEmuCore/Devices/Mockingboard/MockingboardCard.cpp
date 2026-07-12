#include "Pch.h"

#include "MockingboardCard.h"
#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockingboardCard
//
////////////////////////////////////////////////////////////////////////////////

MockingboardCard::MockingboardCard (int slot)
{
    m_slot = slot;
    m_base = static_cast<Word> (kIoBase + slot * kSlotStride);

    m_audioSource[0].SetPsg (&m_psg[0]);
    m_audioSource[1].SetPsg (&m_psg[1]);

    // Mockingboard is dual-mono: PSG #1 hard-left, PSG #2 hard-right.
    m_audioSource[0].SetPan (1.0f, 0.0f);
    m_audioSource[1].SetPan (0.0f, 1.0f);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Decodes the register access, refreshing the VIA's port-A input from the
//  PSG first when the control lines currently select a PSG read so the CPU
//  sees live AY data.
//
////////////////////////////////////////////////////////////////////////////////

Byte MockingboardCard::Read (Word address)
{
    Word   offset = static_cast<Word> ((address - m_base) & (kPageSize - 1));
    int    index  = (offset & kVia2Select) ? 1 : 0;
    Byte   reg    = static_cast<Byte> (offset & Via6522::kRegisterMask);
    Byte   portB  = m_via[index].GetPortB();



    if ((reg == Via6522::kRegOra || reg == Via6522::kRegOraNh) &&
        (portB & kAyResetLow) != 0 &&
        (portB & kAyControlMask) == kAyBc1)
    {
        m_via[index].SetPortAInput (m_psg[index].ReadData());
    }

    return m_via[index].ReadRegister (reg);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Write
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::Write (Word address, Byte value)
{
    Word   offset = static_cast<Word> ((address - m_base) & (kPageSize - 1));
    int    index  = (offset & kVia2Select) ? 1 : 0;
    Byte   reg    = static_cast<Byte> (offset & Via6522::kRegisterMask);



    m_via[index].WriteRegister (reg, value);

    SyncPsg (index);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Reset
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::Reset()
{
    int   i = 0;



    for (i = 0; i < kViaCount; i++)
    {
        m_via[i].Reset();
        m_psg[i].Reset();
        m_lastControl[i] = 0;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SoftReset
//
//  The card's RESET line is tied to the Apple reset line, so Ctrl-Reset
//  silences the PSGs and clears the VIAs.
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::SoftReset()
{
    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PowerCycle
//
//  The Mockingboard has no DRAM-shaped state to randomise, so a cold start
//  is identical to a reset; the Prng required by the MemoryDevice contract
//  is unused.
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::PowerCycle (Prng & prng)
{
    UNREFERENCED_PARAMETER (prng);

    Reset();
}





////////////////////////////////////////////////////////////////////////////////
//
//  Tick
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::Tick (uint32_t cycles)
{
    int   i = 0;



    for (i = 0; i < kViaCount; i++)
    {
        m_via[i].Tick (cycles);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  AttachInterruptController
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MockingboardCard::AttachInterruptController (IInterruptController * ic)
{
    HRESULT   hr = S_OK;
    int       i  = 0;



    for (i = 0; i < kViaCount; i++)
    {
        hr = m_via[i].AttachInterruptController (ic);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SetSampleRate
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::SetSampleRate (uint32_t sampleRate)
{
    int   i = 0;



    for (i = 0; i < kViaCount; i++)
    {
        m_psg[i].SetSampleRate (sampleRate);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncPsg
//
//  Translates the current VIA port state into a PSG bus operation. RESET
//  (PB2 low) clears the chip; otherwise a change in the BDIR/BC1 lines into
//  an active combination latches an address, writes data, or hands PSG data
//  back to the port-A input latch.
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::SyncPsg (int index)
{
    Byte   portB   = m_via[index].GetPortB();
    Byte   portA   = m_via[index].GetPortA();
    Byte   control = static_cast<Byte> (portB & kAyControlMask);



    if ((portB & kAyResetLow) == 0)
    {
        m_psg[index].Reset();
        m_lastControl[index] = 0;
    }
    else if (control != m_lastControl[index])
    {
        switch (control)
        {
        case (kAyBdir | kAyBc1):   // latch register address
            m_psg[index].LatchAddress (portA);
            break;

        case kAyBdir:              // write data to the latched register
            m_psg[index].WriteData (portA);
            break;

        case kAyBc1:               // read data from the latched register
            m_via[index].SetPortAInput (m_psg[index].ReadData());
            break;

        default:                   // inactive
            break;
        }

        m_lastControl[index] = control;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  Create
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> MockingboardCard::Create (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (bus);

    return make_unique<MockingboardCard> (config.slot);
}
