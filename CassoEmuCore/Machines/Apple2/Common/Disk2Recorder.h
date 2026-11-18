#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/Disk2AddressMarkWatcher.h"
#include "Machines/Apple2/Common/Disk2NibbleRecord.h"

class Disk2Drive;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2Recorder
//
//  The debugger's log of a Disk II controller's traffic, from either
//  front end: the address-mark watcher, the recent nibbles, and the access
//  logs in the drives. The instruction address source is the CPU's, for
//  the address of the instruction behind each nibble; with none, as in a
//  test without a CPU, each entry has address 0. It is wiring, not saved.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2Recorder
{
public:
    void  SetInstructionAddressSource (const Word * source) noexcept { m_instructionAddressSource = source; }

    void  NoteRead       (Disk2Drive & drive, int index, Byte nibble, Byte flags, uint64_t cycle);
    void  NoteWrite      (Disk2Drive & drive, int index, Byte nibble, bool isMotorOn);
    void  NoteHeadMoved  (int quarterTrack) { m_watcher.NoteHeadMoved (quarterTrack); }
    void  NoteDiskChange (int index)        { m_record.NoteDiskChange (index); }

    Disk2AddressMarkWatcher        & GetWatcher      ()       { return m_watcher; }
    const Disk2AddressMarkWatcher  & GetWatcher      () const { return m_watcher; }
    Disk2NibbleRecord              & GetNibbleRecord ()       { return m_record; }
    const Disk2NibbleRecord        & GetNibbleRecord () const { return m_record; }

private:
    Word  GetInstructionAddress() const { return (m_instructionAddressSource != nullptr) ? *m_instructionAddressSource : 0; }

    Disk2AddressMarkWatcher    m_watcher;
    Disk2NibbleRecord          m_record;
    const Word               * m_instructionAddressSource = nullptr;
};
