#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"
#include "Core/IInterruptController.h"
#include "Via6522.h"
#include "Ay8910.h"
#include "Ssi263.h"
#include "MockingboardAudioSource.h"
#include "Ssi263AudioSource.h"

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
//  The card comes in two variants matching the product line. The
//  Mockingboard A is the sound half above with two EMPTY speech sockets;
//  the Mockingboard C is the A with one SSI 263A installed. The board
//  decodes a speech socket when A4 is clear, A5 or A6 is set, and A7 is
//  clear -- A6 picks the socket, so chip 1 (the one the C populates)
//  answers at $Cn40-$Cn4F and $Cn60-$Cn6F.
//
//  The board's VIAs see none of A4-A6, so the speech chip is a TAP, not a
//  replacement: a write in a populated speech range reaches BOTH the VIA
//  mirror and the chip (verified against real hardware), and a read there
//  carries the chip's request status on D7 -- the only line the chip
//  drives -- over the VIA mirror's remaining bits. An empty socket leaves
//  its range behaving exactly as the A, which is also what real hardware
//  does.
//
//  The chip's A/R request output is wired to VIA #1's CA1, so speech
//  software takes its ready interrupt through the same VIA IFR/IER path
//  the timers use. A/R is active low: request asserted pulls the line
//  down, and PCR bit 0 = 0 (falling edge) is the setting real drivers use.
//
////////////////////////////////////////////////////////////////////////////////

enum class MockingboardVariant
{
    SoundOnly,      // Mockingboard A: two empty speech sockets
    SoundSpeech,    // Mockingboard C: SSI 263A installed in socket 1
};

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
    static constexpr Byte  kAyBc1         = 0x01;   // PB0
    static constexpr Byte  kAyBdir        = 0x02;   // PB1
    static constexpr Byte  kAyResetLow    = 0x04;   // PB2, active low
    static constexpr Byte  kAyControlMask = kAyBdir | kAyBc1;

    // Speech-socket decode within the slot page: A4 clear, A5 or A6 set,
    // A7 clear. A6 picks the socket.
    static constexpr Word    kSpeechA4     = 0x10;
    static constexpr Word    kSpeechSelect = 0x60;
    static constexpr Word    kSpeechChip1  = 0x40;

    explicit MockingboardCard (int slot,
                               MockingboardVariant variant = MockingboardVariant::SoundOnly);

    // MemoryDevice
    Byte    Read       (Word address) override;
    void    Write      (Word address, Byte value) override;
    Word    GetStart   () const override { return m_base; }
    Word    GetEnd     () const override { return static_cast<Word> (m_base + kPageSize - 1); }
    void    Reset      () override;
    void    SoftReset  () override;
    void    PowerCycle (Prng & prng) override;

    // Advance both VIA timers and the voice chip by `cycles` phi2 clocks
    // (fires timer IRQs).
    void    Tick       (uint32_t cycles);

    // Register both VIAs' IRQ sources with the shared controller.
    HRESULT AttachInterruptController (IInterruptController * ic);

    // Set the host audio sample rate on both PSGs.
    void    SetSampleRate (uint32_t sampleRate);

    int                        GetSlot        () const { return m_slot; }
    MockingboardVariant        GetVariant     () const { return m_variant; }
    Via6522 &                  GetVia         (int index) { return m_via[index]; }
    Ay8910 &                   GetPsg         (int index) { return m_psg[index]; }
    MockingboardAudioSource *  GetAudioSource (int index) { return &m_audioSource[index]; }

    // The installed voice chip, or nullptr on the sound-only variant.
    Ssi263 *  GetSpeech () { return m_speech.get(); }

    // Center-panned speech source; silent whenever the chip is absent or idle.
    Ssi263AudioSource *  GetSpeechAudioSource () { return &m_speechSource; }

    static unique_ptr<MemoryDevice> Create       (const DeviceConfig & config, MemoryBus & bus);
    static unique_ptr<MemoryDevice> CreateSpeech (const DeviceConfig & config, MemoryBus & bus);

private:
    void    SyncPsg            (int index);
    bool    IsInstalledSpeech  (Word offset) const;
    void    SyncSpeechRequest  ();

    int                   m_slot    = 0;
    Word                  m_base    = 0;
    MockingboardVariant   m_variant = MockingboardVariant::SoundOnly;

    unique_ptr<Ssi263>    m_speech;
    Ssi263AudioSource     m_speechSource;

    Via6522                    m_via[kViaCount];
    Ay8910                     m_psg[kViaCount];
    MockingboardAudioSource    m_audioSource[kViaCount];

    // Last control-line state (BDIR|BC1) seen on each VIA, for edge
    // detection of PSG bus operations.
    Byte     m_lastControl[kViaCount] = { 0, 0 };
};
