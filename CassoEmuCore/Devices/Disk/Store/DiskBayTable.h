#pragma once

#include "Pch.h"

#include "Devices/Disk/Store/DiskBay.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBayTable
//
//  The fixed array of bays every store component works on, and the queries
//  that look across all of them.
//
////////////////////////////////////////////////////////////////////////////////

class DiskBayTable
{
public:
    static constexpr int  kSlotCount  = 8;
    static constexpr int  kDriveCount = 2;

    DiskBayTable();

    static bool       IsValidBay                  (int slot, int drive);

    DiskBay         & GetBay                      (int slot, int drive);
    const DiskBay   & GetBay                      (int slot, int drive) const;

    bool              IsFileInAnotherBay          (const string & path, int exceptSlot, int exceptDrive,
                                                   int & outDrive) const;
    bool              IsDirectoryInAnotherBay     (const string & directory, int exceptSlot, int exceptDrive) const;

    uint64_t          TakeNextMediaId             ();

    //  Guards each bay's pending-change record, which a watcher thread writes
    //  while the CPU thread reads it.
    std::mutex      & GetPendingMutex             () const { return m_pendingMutex; }

private:
    DiskBay               m_bays[kSlotCount][kDriveCount];
    uint64_t              m_lastMediaId = 0;
    mutable std::mutex    m_pendingMutex;
};
