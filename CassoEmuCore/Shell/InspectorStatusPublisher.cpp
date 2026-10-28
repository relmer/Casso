#include "Pch.h"

#include "Shell/InspectorStatusPublisher.h"
#include "Machines/Apple2/Common/Disk2Controller.h"





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusPublisher::Get
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DriveHeadState> InspectorStatusPublisher::Get (int drive) const
{
    std::scoped_lock  lock (m_lock);



    return (drive >= 0 && drive < kDriveCount) ? m_states[drive] : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusPublisher::Publish
//
//  Each drive's state built outside the lock, then the pointers swapped
//  under it, so a reader never waits on the build.
//
////////////////////////////////////////////////////////////////////////////////

void InspectorStatusPublisher::Publish (const Disk2Controller * controller, IInspectorDiskSource & source)
{
    std::array<std::shared_ptr<const DriveHeadState>, kDriveCount>  next;
    std::shared_ptr<const DriveHeadState>                           previous;



    if (m_isActive)
    {
        for (int drive = 0; drive < kDriveCount; drive++)
        {
            previous    = Get (drive);
            next[drive] = std::make_shared<const DriveHeadState> (Build (controller, source, drive, previous.get()));
        }

        std::scoped_lock  lock (m_lock);

        m_states = next;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusPublisher::Build
//
//  The selected drive is the one that turns while the motor runs: it reads,
//  writes, or has its write dropped. The counts generation steps whenever a
//  count differs from the last state, or the disk is another.
//
////////////////////////////////////////////////////////////////////////////////

DriveHeadState InspectorStatusPublisher::Build (const Disk2Controller * controller, IInspectorDiskSource & source, int drive, const DriveHeadState * previous)
{
    DriveHeadState         state;
    InspectorDiskIdentity  identity;
    const DiskImage *      image    = source.GetImage (drive, identity);
    int                    slot     = 0;



    state.hasController = controller != nullptr;
    state.hasDisk       = image != nullptr;
    state.mediaId       = identity.mediaId;

    if (controller != nullptr)
    {
        const Disk2NibbleEngine &  engine = controller->GetEngine (drive);

        state.isMotorOn        = controller->IsMotorOn();
        state.isEnabled        = controller->GetActiveDrive() == drive;
        state.isTurning        = state.isMotorOn && state.isEnabled;
        state.headQuarterTrack = engine.GetCurrentTrack();
        state.trackRecord      = engine.GetSlot();
        state.angle            = engine.GetAngle();
        state.readNibbles      = engine.GetReadNibbles();
        state.writeNibbles     = engine.GetWriteNibbles();

        if (state.isTurning && engine.IsWriteMode())
        {
            state.activity = controller->IsWriteBlocked (drive) ? HeadActivity::WriteBlocked : HeadActivity::Writing;
        }
        else if (state.isTurning)
        {
            state.activity = HeadActivity::Reading;
        }

        for (int qt = 0; qt < DiskImage::kQuarterTrackCount; qt++)
        {
            state.visitCounts[qt] = controller->GetVisitCount (drive, qt);
        }

        //  The spans are copied only when the log has changed.
        state.accessGeneration = controller->GetAccessLog (drive).GetGeneration();

        if (previous != nullptr && previous->access != nullptr && previous->accessGeneration == state.accessGeneration)
        {
            state.access = previous->access;
        }
        else
        {
            auto  access = std::make_shared<DriveAccess>();

            access->reads  = controller->GetAccessLog (drive).GetReads();
            access->writes = controller->GetAccessLog (drive).GetWrites();
            state.access   = access;
        }
    }

    if (image != nullptr)
    {
        state.writeProtect = image->GetWriteProtectInfo();
        state.saveCount    = image->GetSaveCount();

        for (slot = 0; slot < image->GetTrackCount(); slot++)
        {
            state.recordWriteCounts.push_back (image->GetGuestWriteCount (slot));
        }
    }

    state.countsGeneration = (previous == nullptr) ? 1 : previous->countsGeneration + (IsSameCounts (state, *previous) ? 0 : 1);

    return state;
}





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusPublisher::IsSameCounts
//
////////////////////////////////////////////////////////////////////////////////

bool InspectorStatusPublisher::IsSameCounts (const DriveHeadState & a, const DriveHeadState & b)
{
    return a.mediaId == b.mediaId && a.recordWriteCounts == b.recordWriteCounts && a.visitCounts == b.visitCounts && a.accessGeneration == b.accessGeneration &&
           a.saveCount == b.saveCount && a.writeProtect.imageFlag == b.writeProtect.imageFlag;
}
