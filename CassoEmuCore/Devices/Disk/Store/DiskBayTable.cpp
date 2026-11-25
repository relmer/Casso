#include "Pch.h"

#include "Devices/Disk/Store/DiskBayTable.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBayTable::DiskBayTable
//
////////////////////////////////////////////////////////////////////////////////

DiskBayTable::DiskBayTable()
{
    int  slot  = 0;
    int  drive = 0;



    //  Each entry learns where it lives, once, so anything holding one can
    //  name the drive without being told.
    for (slot = 0; slot < kSlotCount; slot++)
    {
        for (drive = 0; drive < kDriveCount; drive++)
        {
            m_bays[slot][drive].slot  = slot;
            m_bays[slot][drive].drive = drive;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetBay
//
////////////////////////////////////////////////////////////////////////////////

DiskBay & DiskBayTable::GetBay (int slot, int drive)
{
    return m_bays[slot][drive];
}


const DiskBay & DiskBayTable::GetBay (int slot, int drive) const
{
    return m_bays[slot][drive];
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBayTable::IsFileInAnotherBay
//
//  Whether some OTHER bay is already reading and writing this file.
//
//  EVERY SLOT, NOT JUST THIS ONE. Two controllers are as capable of holding one
//  file between them as two drives on one controller are, and the damage is
//  identical.
//
//  A BAY WITH NO PATH IS NOT A MATCH. An empty bay and a bay built from bytes
//  that never came off disk both carry nothing to compare, and an empty string
//  matches another empty string.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskBayTable::IsFileInAnotherBay (
    const string  & path,
    int             exceptSlot,
    int             exceptDrive,
    int           & outDrive) const
{
    int   slot  = 0;
    int   drive = 0;
    bool  found = false;



    outDrive = -1;

    if (path.empty())
    {
        return false;
    }

    for (slot = 0; slot < kSlotCount && !found; slot++)
    {
        for (drive = 0; drive < kDriveCount && !found; drive++)
        {
            const DiskBay &  other = m_bays[slot][drive];

            if (slot == exceptSlot && drive == exceptDrive)
            {
                continue;
            }

            if (other.mounted && MountedImageState::IsSamePath (other.path, path))
            {
                outDrive = drive;
                found    = true;
            }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsValidBay
//
//  Whether (slot, drive) names a real bay. The one place the fixed array's
//  bounds are stated, so an accessor cannot get the check subtly wrong.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskBayTable::IsValidBay (int slot, int drive)
{
    return slot  >= 0 && slot  < kSlotCount
        && drive >= 0 && drive < kDriveCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBayTable::IsDirectoryInAnotherBay
//
//  Whether a bay other than (exceptSlot, exceptDrive) has a disk mounted from
//  `directory`.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskBayTable::IsDirectoryInAnotherBay (const string & directory, int exceptSlot, int exceptDrive) const
{
    bool  stillUsed  = false;
    int   otherSlot  = 0;
    int   otherDrive = 0;



    for (otherSlot = 0; otherSlot < kSlotCount; otherSlot++)
    {
        for (otherDrive = 0; otherDrive < kDriveCount; otherDrive++)
        {
            bool  isSelf = (otherSlot == exceptSlot && otherDrive == exceptDrive);

            if (!isSelf && m_bays[otherSlot][otherDrive].mounted)
            {
                string  other = MountedImageState::GetDirectory (m_bays[otherSlot][otherDrive].path);

                stillUsed = stillUsed || MountedImageState::IsSamePath (other, directory);
            }
        }
    }

    return stillUsed;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBayTable::TakeNextMediaId
//
//  A media id no bay has had before in this session.
//
////////////////////////////////////////////////////////////////////////////////

uint64_t DiskBayTable::TakeNextMediaId()
{
    return ++m_lastMediaId;
}





