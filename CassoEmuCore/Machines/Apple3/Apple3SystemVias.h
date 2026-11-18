#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Devices/Via6522.h"

class Apple3Mmu;
class IInterruptController;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3SystemVias
//
//  The Apple ///'s two 6522s at $FFD0-$FFEF and their wiring. The D VIA's
//  port A is the environment register and its port B the zero page
//  register; the E VIA's port A has the bank in bits 0-3, the slot 4 and
//  slot 3 interrupt requests in bits 4 and 5, the Apple II lockout in bit 6
//  and the interrupt line in bit 7. After each write the port outputs are
//  passed to the memory map.
//
//  An input pin with nothing driving it reads high, as the pull-ups on the
//  board make it, so after a reset the environment register reads $FF and
//  the ROM is selected.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3SystemVias : public MemoryDevice, public IMachineState
{
public:
    static constexpr Word      kFirst            = 0xFFD0;
    static constexpr Word      kLast             = 0xFFEF;
    static constexpr Word      kFirstE           = 0xFFE0;

    static constexpr Byte      kBankBits         = 0x0F;
    static constexpr Byte      kSlot4Irq         = 0x10;
    static constexpr Byte      kSlot3Irq         = 0x20;
    static constexpr Byte      kLockout          = 0x40;
    static constexpr Byte      kIrqLine          = 0x80;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'V', 'I');
    static constexpr uint16_t  kStateVersion = 1;

    explicit Apple3SystemVias (Apple3Mmu & mmu);

    HRESULT  AttachInterruptController (IInterruptController * ic);

    // MemoryDevice: $FFD0-$FFEF, accessed through the memory map.
    Byte     Read      (Word address) override;
    void     Write     (Word address, Byte value) override;
    Word     GetStart  () const override { return kFirst; }
    Word     GetEnd    () const override { return kLast;  }
    void     Reset     () override;
    bool     TryPeekIo (Word address, Byte & value) override;

    //  Advances both VIAs' timers by ticks of their 1 MHz clock.
    void     Tick      (uint32_t cycles);

    //  The CPU's accesses since the last call, each a cycle at 1 MHz.
    uint32_t TakeAccessCount () { uint32_t count = m_accesses; m_accesses = 0; return count; }

    //  Input lines. A request is asserted low on the board; these take
    //  whether it is asserted.
    void     SetSlotInterrupt (int slot, bool asserted);
    void     SetInterruptLine (bool asserted);

    Via6522        & GetD ()       { return m_d; }
    Via6522        & GetE ()       { return m_e; }
    const Via6522  & GetD () const { return m_d; }
    const Via6522  & GetE () const { return m_e; }

    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

private:
    static constexpr Byte  kPulledUp = 0xFF;

    Via6522  & GetVia           (Word address) { return (address >= kFirstE) ? m_e : m_d; }
    void       UpdateOutputs    ();
    void       SetPortAInputBit (Byte bit, bool high);

    Apple3Mmu  & m_mmu;
    Via6522      m_d;
    Via6522      m_e;
    Byte         m_eInputA      = kPulledUp;
    Byte         m_slotRequests = 0;            // bit n - 1 set while slot n requests
    uint32_t     m_accesses     = 0;
};
