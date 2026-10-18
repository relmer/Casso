#include "Pch.h"

#include "DriveStatus.h"


static const BayStatus      s_kEmptyBay;
static const DriveActivity  s_kNoActivity;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveStatus
//
////////////////////////////////////////////////////////////////////////////////

DriveStatus::DriveStatus() :
    m_bays (std::make_shared<const BayTable>())
{
}


DriveStatus::DriveStatus (
    uint64_t                          sequence,
    uint64_t                          bayGeneration,
    std::shared_ptr<const BayTable>   bays,
    bool                              hasController,
    const ActivityTable             & activity) :
    m_sequence      (sequence),
    m_bayGeneration (bayGeneration),
    m_bays          (std::move (bays)),
    m_hasController (hasController),
    m_activity      (activity)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBay
//
//  An empty bay for a slot or drive out of range.
//
////////////////////////////////////////////////////////////////////////////////

const BayStatus & DriveStatus::GetBay (int slot, int drive) const
{
    bool  isValid = slot  >= 0 && slot  < DiskImageStore::kSlotCount &&
                    drive >= 0 && drive < DiskImageStore::kDriveCount;



    return isValid ? (*m_bays)[slot * DiskImageStore::kDriveCount + drive] : s_kEmptyBay;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetActivity
//
//  Slot 6's drive, or no activity for a drive out of range.
//
////////////////////////////////////////////////////////////////////////////////

const DriveActivity & DriveStatus::GetActivity (int drive) const
{
    bool  isValid = drive >= 0 && drive < DiskImageStore::kDriveCount;



    return isValid ? m_activity[drive] : s_kNoActivity;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMountedSources
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DiskImageStore::MountedSource> DriveStatus::GetMountedSources() const
{
    std::vector<DiskImageStore::MountedSource>  sources;
    int                                         slot  = 0;
    int                                         drive = 0;



    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            const BayStatus &  bay = GetBay (slot, drive);

            if (bay.isMounted && !bay.path.empty())
            {
                sources.push_back ({ bay.path, slot, drive });
            }
        }
    }

    return sources;
}