#pragma once

#include "Pch.h"

#include "BayStatus.h"
#include "DiskImageStore.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveActivity
//
//  One slot 6 drive's motor, head and nibble counters at one instant.
//  headQuarterTrack is -1 when there is no controller.
//
////////////////////////////////////////////////////////////////////////////////

struct DriveActivity
{
    bool      isMotorOn        = false;
    int       headQuarterTrack = -1;
    uint64_t  readNibbles      = 0;
    uint64_t  writeNibbles     = 0;

    bool  operator== (const DriveActivity &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DriveStatus
//
//  Every bay and the slot 6 drives' activity, built at one instant on the
//  thread that owns the disk store and published whole. Snapshots taken
//  between two bay changes share one bay table.
//
////////////////////////////////////////////////////////////////////////////////

class DriveStatus
{
public:
    using BayTable      = std::array<BayStatus, DiskImageStore::kSlotCount * DiskImageStore::kDriveCount>;
    using ActivityTable = std::array<DriveActivity, DiskImageStore::kDriveCount>;

    DriveStatus ();
    DriveStatus (uint64_t                          sequence,
                 uint64_t                          bayGeneration,
                 std::shared_ptr<const BayTable>   bays,
                 bool                              hasController,
                 const ActivityTable             & activity);

    uint64_t               GetSequence      () const { return m_sequence; }
    uint64_t               GetBayGeneration () const { return m_bayGeneration; }
    bool                   HasController    () const { return m_hasController; }
    const BayStatus      & GetBay           (int slot, int drive) const;
    const DriveActivity  & GetActivity      (int drive) const;

    //  The mounted bays with a file behind them, as GetMountedSourcePaths
    //  returns them.
    std::vector<DiskImageStore::MountedSource>  GetMountedSources () const;

private:
    uint64_t                         m_sequence      = 0;
    uint64_t                         m_bayGeneration = 0;
    std::shared_ptr<const BayTable>  m_bays;
    bool                             m_hasController = false;
    ActivityTable                    m_activity      {};
};