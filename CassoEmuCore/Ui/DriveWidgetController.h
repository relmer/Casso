#pragma once

#include "Pch.h"

struct DriveWidgetState;
class IDriveCommandSink;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveWidgetController
//
//  Host-side adapter between `EmulatorShell` (which owns the per-drive
//  `DriveWidgetState` and implements `IDriveCommandSink`) and the
//  native chrome drive-widget tree. The widget tree itself is
//  reintroduced in a later phase; in this baseline only the state-pump
//  + sync-event channel is live so the existing mount/eject paths
//  continue to publish events the future chrome will consume.
//
//  Any thread may publish a sync event; the UI frame consumes them.
//
////////////////////////////////////////////////////////////////////////////////

class DriveWidgetController
{
public:
    enum class SyncAction
    {
        DoorOpen,
        DoorClose,
        DoorReinsert,
        SpinStart,
        SpinStop,
    };

    struct DriveSyncEvent
    {
        uint64_t   eventId     = 0;
        int        driveId     = 0;
        SyncAction action      = SyncAction::DoorOpen;
        int64_t    timestampMs = 0;
    };


    DriveWidgetController  ();
    ~DriveWidgetController ();

    void                        RegisterInstancer ();
    HRESULT                     LoadDocument      (IDriveCommandSink * pSink,
                                                   HWND                ownerHwnd);
    void                        UnloadDocument    ();
    void                        SyncFromStates    (const std::array<DriveWidgetState, 2> & states);
    void                      * HitTest           (int clientX, int clientY) const;
    uint64_t                    PublishSyncEvent  (int driveId, SyncAction action, int64_t timestampMs);
    std::vector<DriveSyncEvent> ConsumeSyncEvents ();

    size_t                      GetWidgetCount    () const { return 0; }

private:
    //  Published from the CPU thread, as its disk changes reach OnBayChange,
    //  and drained by the UI frame. The mutex guards the queue and the id
    //  counter both, so ids enter the queue in the order they are given out.
    std::mutex                   m_syncMutex;
    std::vector<DriveSyncEvent>  m_syncEvents;
    uint64_t                     m_nextSyncEventId = 1;
};
