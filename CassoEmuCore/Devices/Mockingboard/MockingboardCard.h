#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Core/IInterruptController.h"
#include "Via6522.h"
#include "Ay8910.h"
#include "MockingboardAudioSource.h"

class MemoryBus;

struct DeviceConfig;




////////////////////////////////////////////////////////////////////////////////
//
//  MockingboardCard
//
//  Sweet Micro Systems Mockingboard A/C: two 6522 VIAs, each wired to one
//  AY-3-8910 PSG, occupying a slot's $Cn00 I/O-select page. Within the
//  page, address bit 7 selects the VIA (0 -> VIA #1 / PSG #1 at $Cn00,
//  1 -> VIA #2 / PSG #2 at $Cn80); the low four address bits select the
//  VIA register, so the register file mirrors every 16 bytes.
//
//  Each VIA drives its PSG over the standard bus wiring: port A is the
//  8-bit AY data bus, and port B's low three bits are the control lines
//  BC1 (PB0), BDIR (PB1), and active-low RESET (PB2). After every register
//  write the card re-evaluates those lines and, on the edge into an active
//  state, latches an address / writes data / reads data on the PSG.
//
//  Timer 1 in continuous mode is the interrupt source Mockingboard music
//  players use for tempo; both VIAs register their IRQ with the shared
//  interrupt controller. Each PSG feeds a MockingboardAudioSource (PSG #1
//  hard-left, PSG #2 hard-right) that the host registers with its audio
//  mixer.
//
////////////////////////////////////////////////////////////////////////////////

class MockingboardCard : public MemoryDevice
{
public:
    static constexpr int     kViaCount   = 2;
    static constexpr Word    kIoBase     = 0xC000;
    static constexpr Word    kSlotStride = 0x100;
    static constexpr Word    kPageSize   = 0x100;

    // Address bit 7 selects the second VIA/PSG within the slot page.
    static constexpr Word    kVia2Select = 0x80;

    // AY control lines on VIA port B.
    static constexpr Byte    kAyBc1      = 0x01;   // PB0
    static constexpr Byte    kAyBdir     = 0x02;   // PB1
    static constexpr Byte    kAyResetLow = 0x04;   // PB2, active low
    static constexpr Byte    kAyControlMask = kAyBdir | kAyBc1;

    explicit MockingboardCard (int slot);

    // MemoryDevice
    Byte    Read       (Word address) override;
    void    Write      (Word address, Byte value) override;
    Word    GetStart   () const override { return m_base; }
    Word    GetEnd     () const override { return static_cast<Word> (m_base + kPageSize - 1); }
    void    Reset      () override;
    void    SoftReset  () override;
    void    PowerCycle (Prng & prng) override;

    // Advance both VIA timers by `cycles` phi2 clocks (fires timer IRQs).
    void    Tick       (uint32_t cycles);

    // Register both VIAs' IRQ sources with the shared controller.
    HRESULT AttachInterruptController (IInterruptController * ic);

    // Set the host audio sample rate on both PSGs.
    void    SetSampleRate (uint32_t sampleRate);

    int                        GetSlot        () const { return m_slot; }
    Via6522 &                  GetVia         (int index) { return m_via[index]; }
    Ay8910 &                   GetPsg         (int index) { return m_psg[index]; }
    MockingboardAudioSource *  GetAudioSource (int index) { return &m_audioSource[index]; }

    static unique_ptr<MemoryDevice> Create (const DeviceConfig & config, MemoryBus & bus);

private:
    void    SyncPsg (int index);

    int      m_slot = 0;
    Word     m_base = 0;

    Via6522                    m_via[kViaCount];
    Ay8910                     m_psg[kViaCount];
    MockingboardAudioSource    m_audioSource[kViaCount];

    // Last control-line state (BDIR|BC1) seen on each VIA, for edge
    // detection of PSG bus operations.
    Byte     m_lastControl[kViaCount] = { 0, 0 };
};
