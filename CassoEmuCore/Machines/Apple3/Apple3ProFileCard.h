#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"
#include "Core/MemoryDevice.h"
#include "Devices/BlockDevice/ProFileDrive.h"

class MemoryBus;





////////////////////////////////////////////////////////////////////////////////
//
//  Apple3ProFileCard
//
//  The ProFile's interface card in an Apple /// slot, from Apple's SOS
//  .PROFILE driver 1.30. Its registers are at $C080 plus 16 times the slot:
//
//      +0   write: a byte to the drive
//      +1   read: a byte from the drive
//      +2   read: status; bit 7 is 1 while the drive's BSY line is idle,
//           and bit 6 is a parity error
//      +3   write: clears the parity error
//
//  Reads of $Cn00-$Cn0F set a latch of the card's lines: bits 0 and 1 of the
//  offset select the line and bit 2 is its level, and with bit 3 set the
//  offset is the drive's reset line instead. Line 0 is CMD: raising it
//  starts a handshake and dropping it ends one. The direction and interrupt
//  lines have no effect here, and the parity error bit is always 0.
//
////////////////////////////////////////////////////////////////////////////////

class Apple3ProFileCard : public MemoryDevice, public IMachineState
{
public:
    static constexpr Byte      kStatusIdle   = 0x80;
    static constexpr Byte      kCmdLow       = 0x00;
    static constexpr Byte      kCmdHigh      = 0x04;
    static constexpr Byte      kResetHigh    = 0x0C;
    static constexpr Byte      kResetLow     = 0x08;

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('A', '3', 'P', 'F');
    static constexpr uint16_t  kStateVersion = 1;

    Apple3ProFileCard (int slot, MemoryBus & bus);

    // MemoryDevice: the slot's device I/O and its $Cn00 page.
    Byte  Read       (Word address) override;
    void  Write      (Word address, Byte value) override;
    Word  GetStart   () const override { return m_ioStart; }
    Word  GetEnd     () const override { return m_ioEnd; }
    void  Reset      () override;
    bool  TryPeekIo  (Word address, Byte & value) override;

    void            SetDrive  (std::unique_ptr<ProFileDrive> drive) { m_drive = std::move (drive); }
    ProFileDrive  & GetDrive  () { return *m_drive; }
    int             GetSlot   () const { return m_slot; }
    Byte            GetLines  () const { return m_lines; }

    //  IMachineState: the line latch and the drive, its written blocks
    //  included.
    HRESULT  SaveState (StateWriter & writer) const override;
    HRESULT  LoadState (StateReader & reader) override;

private:
    bool  IsSlotPage   (Word address) const;
    Byte  PeekIo       (Word address) const;
    void  OnLineAccess (Byte offset);

    int                             m_slot    = 0;
    Word                            m_ioStart = 0;
    Word                            m_ioEnd   = 0;
    MemoryBus                     & m_bus;
    std::unique_ptr<ProFileDrive>   m_drive;
    Byte                            m_lines   = 0;
};
