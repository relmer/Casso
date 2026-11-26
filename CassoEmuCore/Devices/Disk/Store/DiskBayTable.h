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

    //  Reverse execution keeps guest writes in memory while it records, and a
    //  replay re-runs a past the host already saw. The store's components
    //  check these before writing a file or taking up a changed one.
    void              SetFlushHold                (bool isHeld)      { m_isFlushHeld = isHeld; }
    bool              IsFlushHeld                 () const           { return m_isFlushHeld; }
    void              SetReplaying                (bool isReplaying) { m_isReplaying = isReplaying; }
    bool              IsReplaying                 () const           { return m_isReplaying; }

    //  Called after a disk is mounted, ejected, swapped for a changed file, or
    //  has its write protection changed in its file, except during a replay.
    void              SetMediaChangeListener      (std::function<void ()> listener) { m_mediaChangeListener = std::move (listener); }
    void              NotifyMediaChanged          () const;

private:
    DiskBay                  m_bays[kSlotCount][kDriveCount];
    uint64_t                 m_lastMediaId = 0;
    mutable std::mutex       m_pendingMutex;
    std::function<void ()>   m_mediaChangeListener;
    bool                     m_isFlushHeld = false;
    bool                     m_isReplaying = false;
};
