#pragma once

#include "Pch.h"

#include "Core/MemoryDevice.h"

class Acia6551;
class Apple3AnalogInputs;
class Apple3DiskController;
class Apple3Keyboard;
class Apple3Mmu;
class Mm58167;
class Apple3Sound;
class Apple3Slots;
class Apple3Video;
class MemoryBus;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3IoSpace
//
//  The Apple ///'s I/O space while I/O is on, which the memory map calls for
//  $C000-$CFFF: the keyboard at $C000-$C01F, the display switches at
//  $C050-$C057, the A/D at $C058-$C05F, the switch register at $C06x, the
//  6551 at $C0Fx, the slots at $C020 and
//  $C090-$C0CF and in the slot ROM spaces, the drive select latches and the
//  smooth scroll and character download switches at $C0Dx, and the disk
//  register at $C0Ex. An address with nothing behind it yet reads the
//  floating bus.
//
//  $C030-$C03F toggles the speaker and $C040-$C04D starts the beeper.
//  Any address in $C070-$C07F is the clock chip register the zero page
//  register selects. An access to the clock chip or the 6551 at $C0Fx runs
//  at 1 MHz and is counted for the time base.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3IoSpace : public MemoryDevice
{
public:
    static constexpr Word  kFirst = 0xC000;
    static constexpr Word  kLast  = 0xCFFF;

    Apple3IoSpace (MemoryBus & bus, Apple3Mmu & mmu, Apple3Keyboard & keyboard, Apple3Video & video, Apple3Slots & slots, Apple3DiskController & disk,
                   Apple3AnalogInputs & analog, Acia6551 & acia, Mm58167 & clock, Apple3Sound & sound);

    Byte      Read            (Word address) override;
    void      Write           (Word address, Byte value) override;
    Word      GetStart        () const override { return kFirst; }
    Word      GetEnd          () const override { return kLast;  }
    void      Reset           () override {}
    bool      TryPeekIo       (Word address, Byte & value) override;

    //  The 1 MHz accesses since the last call.
    uint32_t  TakeSlowAccessCount () { uint32_t count = m_slowAccesses; m_slowAccesses = 0; return count; }

private:
    Byte      Access          (Word address, bool isWrite, Byte value);
    Byte      ReadSwitch      (Word address) const;
    bool      IsSlowAddress   (Word address) const;

    MemoryBus             & m_bus;
    Apple3Mmu             & m_mmu;
    Apple3Keyboard        & m_keyboard;
    Apple3Video           & m_video;
    Apple3Slots           & m_slots;
    Apple3DiskController  & m_disk;
    Apple3AnalogInputs    & m_analog;
    Acia6551              & m_acia;
    Mm58167               & m_clock;
    Apple3Sound           & m_sound;
    uint32_t                m_slowAccesses = 0;
};
