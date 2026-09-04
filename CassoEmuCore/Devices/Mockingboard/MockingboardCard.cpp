#include "Pch.h"

#include "MockingboardCard.h"
#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MockingboardCard
//
////////////////////////////////////////////////////////////////////////////////

MockingboardCard::MockingboardCard (int slot, MockingboardVariant variant)
{
    m_slot    = slot;
    m_base    = static_cast<Word> (kIoBase + slot * kSlotStride);
    m_variant = variant;

    m_audioSource[0].SetPsg (&m_psg[0]);
    m_audioSource[1].SetPsg (&m_psg[1]);

    // Mockingboard is dual-mono: PSG #1 hard-left, PSG #2 hard-right.
    m_audioSource[0].SetPan (1.0f, 0.0f);
    m_audioSource[1].SetPan (0.0f, 1.0f);

    // The card wires both PSGs to the Apple II system clock. Stating it here
    // keeps the machine's rate in one place instead of leaving each chip
    // holding its own copy of the number.
    m_psg[0].SetClock (static_cast<double> (kAppleCpuClock));
    m_psg[1].SetClock (static_cast<double> (kAppleCpuClock));

    // The sound+speech variant installs the voice chip in socket 1. The
    // sound-only variant allocates nothing, so it cannot even accidentally
    // acquire speech behavior.
    if (variant == MockingboardVariant::SoundSpeech)
    {
        m_speech = make_unique<Ssi263>();

        // Tick is fed the CPU's cycle counts, so that -- not the voice chip's
        // own XCK -- is the clock its phoneme countdown has to be measured in.
        m_speech->SetTickClock (static_cast<double> (kAppleCpuClock));
    }

    m_speechSource.SetSpeech (m_speech.get());
}





////////////////////////////////////////////////////////////////////////////////
//
//  Read
//
//  Decodes the register access, refreshing the VIA's port-A input from the
//  PSG first when the control lines currently select a PSG read so the CPU
//  sees live AY data.
//
//  Every read on this card is a VIA read. The voice chip is decoded for writes
//  only and drives nothing onto the data bus, so $Cn40-$Cn44 read back as VIA
//  #1's registers reached through the third mirror -- $Cn40 is its port B, and
//  what comes back is whatever the speech writes themselves left in ORB, gated
//  by the DDRB those same writes set. Reading A/R back is a Phasor native mode
//  facility and this card is not a Phasor. Substituting the request bit into
//  D7 here would be a courtesy hardware does not extend, and software written
//  against it would hang on a real board.
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
//  The VIA path always executes -- the board's VIAs see none of A4-A6, so a
//  speech-range write lands in the VIA mirror on real hardware too. When the
//  offset decodes to an installed voice chip, the same write ALSO reaches the
//  chip: a tap, not a replacement.
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::Write (Word address, Byte value)
{
    Word   offset = static_cast<Word> ((address - m_base) & (kPageSize - 1));
    int    index  = (offset & kVia2Select) ? 1 : 0;
    Byte   reg    = static_cast<Byte> (offset & Via6522::kRegisterMask);



    m_via[index].WriteRegister (reg, value);

    SyncPsg (index);

    if (IsInstalledSpeech (offset))
    {
        m_speech->WriteRegister (static_cast<Byte> (offset), value);

        SyncSpeechRequest();
    }
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

    if (m_speech != nullptr)
    {
        m_speech->Reset();

        SyncSpeechRequest();
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
//  Advances the card by `cycles` phi2 clocks. Both the VIA timers and the
//  voice chip's phoneme countdown are in that domain; the voice chip's own XCK
//  is a separate clock and never counts here.
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::Tick (uint32_t cycles)
{
    int   i = 0;



    for (i = 0; i < kViaCount; i++)
    {
        m_via[i].Tick (cycles);
    }

    if (m_speech != nullptr)
    {
        m_speech->Tick (cycles);

        SyncSpeechRequest();
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

    if (m_speech != nullptr)
    {
        m_speech->SetSampleRate (sampleRate);
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





////////////////////////////////////////////////////////////////////////////////
//
//  CreateSpeech
//
//  Factory for the sound+speech variant. A separate registered type rather
//  than a config field, so machine profiles select a card model the way a
//  buyer did -- by product name.
//
////////////////////////////////////////////////////////////////////////////////

unique_ptr<MemoryDevice> MockingboardCard::CreateSpeech (const DeviceConfig & config, MemoryBus & bus)
{
    UNREFERENCED_PARAMETER (bus);

    return make_unique<MockingboardCard> (config.slot, MockingboardVariant::SoundSpeech);
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsInstalledSpeech
//
//  True when the page offset decodes to a speech socket that actually holds
//  a chip. The board selects a socket when A4 is clear, A5 or A6 is set, and
//  A7 is clear; A6 picks the socket. Only socket 1 is populated on the
//  sound+speech variant, and no socket exists to decode on the sound-only
//  variant -- so the A takes one branch here and nothing else changes.
//
////////////////////////////////////////////////////////////////////////////////

bool MockingboardCard::IsInstalledSpeech (Word offset) const
{
    if (m_speech == nullptr)
    {
        return false;
    }

    return (offset & kSpeechA4) == 0 &&
           (offset & kVia2Select) == 0 &&
           (offset & kSpeechChip1) != 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  SyncSpeechRequest
//
//  Drives the voice chip's A/R output onto VIA #2's CA1. A/R is active low:
//  a pending request pulls the line down, so with PCR bit 0 clear (falling
//  edge) the request's assertion latches the CA1 interrupt flag.
//
//  The SECOND VIA, for the socket at $Cn40 -- each speech chip interrupts
//  through the OTHER channel's 6522. The board wires the $Cn20 socket's A/R
//  (pin 4) to VIA #1's CA1 and the $Cn40 socket's to VIA #2's, which is what
//  makes the handshake usable at all: speech writes alias onto VIA #1, so a
//  driver polling the flag for the $Cn40 chip would otherwise be clobbering
//  the register it was watching. Sweet Micro's own speech driver arms
//  $Cn0C/$Cn0D/$Cn0E for its $Cn20 chip, which leaves $Cn8C/$Cn8D/$Cn8E for
//  this one.
//
////////////////////////////////////////////////////////////////////////////////

void MockingboardCard::SyncSpeechRequest()
{
    m_via[1].SetCa1 (!m_speech->IsRequesting());
}
