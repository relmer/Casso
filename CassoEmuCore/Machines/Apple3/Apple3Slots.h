#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"

class Apple3SystemVias;
class MemoryBus;
class MemoryDevice;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3Slots
//
//  The Apple ///'s four peripheral slots, for the I/O space: each
//  slot's device I/O at $C090-$C0CF and memory space at $C100-$C4FF, the
//  shared $C800-$CFFF space, selected by the last access to a slot's memory
//  space, $C020 to deselect it, and each slot's interrupt request.
//  A read of an empty slot returns the floating bus, and a write to one is
//  discarded.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3Slots : public IMachineState
{
public:
    static constexpr int   kSlotCount       = 4;
    static constexpr Word  kFirstSlotIo     = 0xC090;
    static constexpr Word  kLastSlotIo      = 0xC0CF;
    static constexpr Word  kFirstSlotRom    = 0xC100;
    static constexpr Word  kLastSlotRom     = 0xC4FF;
    static constexpr Word  kFirstSharedRom  = 0xC800;
    static constexpr Word  kLastSharedRom   = 0xCFFF;
    static constexpr Word  kDeselect        = 0xC020;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'S', 'L');
    static constexpr uint16_t  kStateVersion = 1;

    Apple3Slots (MemoryBus & bus, Apple3SystemVias & vias);

    void   SetCard            (int slot, MemoryDevice * card);
    void   SetInterrupt       (int slot, bool asserted);
    bool   IsRequesting       (int slot) const;
    int    GetSelectedSlot    () const { return m_selected; }

    Byte   Read               (Word address);
    void   Write              (Word address, Byte value);
    bool   IsSlotAddress      (Word address) const;

    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

private:
    int             GetSlotOf          (Word address) const;
    MemoryDevice  * GetCardFor         (Word address);

    MemoryBus                              & m_bus;
    Apple3SystemVias                       & m_vias;
    std::array<MemoryDevice *, kSlotCount>   m_cards    = {};
    std::array<bool, kSlotCount>             m_requests = {};
    int                                      m_selected = 0;      // 0 for none
};
