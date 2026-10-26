#include "Pch.h"

#include "DriveStatusPublisher.h"

#include "Devices/Disk/DiskImageStore.h"
#include "Machines/Apple2/Common/Disk2Controller.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveStatusPublisher
//
//  Publishes an empty machine, so Take is never null.
//
////////////////////////////////////////////////////////////////////////////////

DriveStatusPublisher::DriveStatusPublisher() :
    m_bays      (std::make_shared<const DriveStatus::BayTable>()),
    m_published (std::make_shared<const DriveStatus>())
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  Publish
//
//  Samples the activity every time. A bay change publishes at once; a change
//  of activity alone publishes only when it is due. In Debug, an unmoved
//  generation rebuilds the bays anyway and asserts nothing changed, so a bay
//  writer that skips MarkStatusChanged fails the suite.
//
////////////////////////////////////////////////////////////////////////////////

DriveStatusChange DriveStatusPublisher::Publish (
    const DiskImageStore  & store,
    const Disk2Controller * controller,
    bool                    isActivityDue)
{
    HRESULT                             hr              = S_OK;
    const ThreadOwnership             & ownership       = store.GetThreadOwnership();
    DriveStatus::ActivityTable          activity        {};
    bool                                hasController   = controller != nullptr;
    bool                                isBaysMoved     = false;
    bool                                isActivityMoved = false;
    DriveStatusChange                   change          = DriveStatusChange::None;
    std::shared_ptr<const DriveStatus>  next;



    ASSERT_THREAD_OWNERSHIP (ownership);

    SampleActivity (controller, activity);

    isBaysMoved     = store.GetStatusGeneration() != m_bayGeneration;
    isActivityMoved = activity != m_activity || hasController != m_hasController;

#if defined(DBG) || defined(DEBUG) || defined(_DEBUG)
    if (!isBaysMoved)
    {
        DriveStatus::BayTable  rebuilt;
        bool                   isSame = false;

        BuildBays (store, rebuilt);
        isSame = rebuilt == *m_bays;
        ASSERT (isSame);
    }
#endif

    BAIL_OUT_IF (!isBaysMoved && !(isActivityMoved && isActivityDue), S_OK);

    if (isBaysMoved)
    {
        auto  bays = std::make_shared<DriveStatus::BayTable>();

        BuildBays (store, *bays);

        m_bays          = std::move (bays);
        m_bayGeneration = store.GetStatusGeneration();
    }

    m_activity      = activity;
    m_hasController = hasController;
    change          = isBaysMoved ? DriveStatusChange::Bays : DriveStatusChange::Activity;

    next = std::make_shared<const DriveStatus> (++m_sequence, m_bayGeneration, m_bays, m_hasController, m_activity);

    {
        std::lock_guard<std::mutex>  lock (m_mutex);



        m_published.swap (next);
    }

Error:
    return change;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Take
//
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<const DriveStatus> DriveStatusPublisher::Take() const
{
    std::lock_guard<std::mutex>  lock (m_mutex);



    return m_published;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BuildBays
//
////////////////////////////////////////////////////////////////////////////////

void DriveStatusPublisher::BuildBays (const DiskImageStore & store, DriveStatus::BayTable & out)
{
    int  slot  = 0;
    int  drive = 0;



    for (slot = 0; slot < DiskImageStore::kSlotCount; slot++)
    {
        for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
        {
            out[slot * DiskImageStore::kDriveCount + drive] = store.GetBayStatus (slot, drive);
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  SampleActivity
//
//  Slot 6's drives: motor, head and nibble counters. With no controller,
//  every drive is idle with no head position.
//
////////////////////////////////////////////////////////////////////////////////

void DriveStatusPublisher::SampleActivity (const Disk2Controller * controller, DriveStatus::ActivityTable & out)
{
    int  drive = 0;



    for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
    {
        DriveActivity  activity;

        if (controller != nullptr)
        {
            const Disk2NibbleEngine &  engine = controller->GetEngine (drive);

            activity.isMotorOn        = engine.IsMotorOn();
            activity.headQuarterTrack = engine.GetCurrentTrack();
            activity.readNibbles      = engine.GetReadNibbles();
            activity.writeNibbles     = engine.GetWriteNibbles();
        }

        out[drive] = activity;
    }
}