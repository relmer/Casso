#pragma once

#include "Pch.h"

#include "Core/IMachineState.h"

class StateReader;
class StateWriter;





////////////////////////////////////////////////////////////////////////////////
//
//  Disk2NibbleRecord
//
//  The most recent nibbles the CPU took from the drives or wrote to them, in
//  the machine's saved state, so a field under way at a snapshot can be
//  decoded from the snapshot alone. Each entry is the nibble, its flags and
//  the address of the instruction that read or wrote it: one per nibble the
//  drive assembled and the CPU read, and one per load of the data latch
//  while the motor runs. The count is of every entry added since the last
//  power cycle; the ring has room for the newest kCapacity.
//
//  The first entry for a drive after its disk changed has kNewDisk set, so
//  no field is decoded across a mount, an eject or a swap.
//
////////////////////////////////////////////////////////////////////////////////

class Disk2NibbleRecord
{
public:
    static constexpr size_t  kCapacity = 512;

    static constexpr Byte  kDrive2   = 0x01;      // the entry is drive 2's
    static constexpr Byte  kWritten  = 0x02;      // a latch load, not a read
    static constexpr Byte  kSpinUp   = 0x04;      // read during the motor's spin-up
    static constexpr Byte  kDropped  = 0x08;      // a load write protection dropped
    static constexpr Byte  kNewDisk  = 0x10;      // the first entry since the drive's disk changed
    static constexpr Byte  kAfterGap = 0x20;      // the drive assembled a nibble the CPU did not take before this one

    struct Entry
    {
        Byte  nibble  = 0;
        Byte  flags   = 0;
        Word  address = 0;
    };

    //  Inline: the controller adds one for every nibble the CPU takes.
    void      Add            (int drive, Byte nibble, Byte flags, Word address)
    {
        Byte     driveBit = static_cast<Byte> (1 << drive);
        Entry  & entry    = m_entries[m_count & kIndexMask];



        entry.nibble     = nibble;
        entry.flags      = static_cast<Byte> (flags | ((drive != 0) ? kDrive2 : 0) | (((m_pendingNewDisk & driveBit) != 0) ? kNewDisk : 0));
        entry.address    = address;
        m_pendingNewDisk = static_cast<Byte> (m_pendingNewDisk & ~driveBit);
        m_count++;
    }

    //  A nibble the CPU took, with the drive's count of nibbles assembled: a
    //  count more than one past the last read's means the CPU missed some.
    void      AddRead        (int drive, Byte nibble, Byte flags, Word address, uint64_t assembled)
    {
        Byte  gap = (assembled - m_lastAssembled[drive] > 1) ? kAfterGap : 0;



        m_lastAssembled[drive] = assembled;
        Add (drive, nibble, static_cast<Byte> (flags | gap), address);
    }

    void      NoteDiskChange (int drive)         { m_pendingNewDisk = static_cast<Byte> (m_pendingNewDisk | (1 << drive)); }
    void      Clear();

    //  Entries added since the last power cycle, and how many the ring has:
    //  those have the indices GetCount() - GetSize() to GetCount() - 1.
    uint64_t  GetCount() const                  { return m_count; }
    size_t    GetSize() const                   { return (size_t) std::min<uint64_t> (m_count, kCapacity); }
    const Entry &  GetEntry (uint64_t index) const { return m_entries[index & kIndexMask]; }

    HRESULT   SaveState (StateWriter & writer) const;
    HRESULT   LoadState (StateReader & reader);

    static constexpr uint32_t  kStateTag     = IMachineState::MakeTag ('D', '2', 'N', 'R');
    static constexpr uint16_t  kStateVersion = 1;

private:
    static constexpr uint64_t  kIndexMask = kCapacity - 1;

    static_assert ((kCapacity & kIndexMask) == 0, "the capacity is a power of two");

    std::array<Entry, kCapacity>  m_entries        = {};
    uint64_t                      m_count          = 0;
    Byte                          m_pendingNewDisk = 0;      // a bit per drive
    std::array<uint64_t, 2>       m_lastAssembled  = {};     // each drive's count of assembled nibbles at its last read
};
