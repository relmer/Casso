#pragma once

#include "Pch.h"

#include "DriveWidgetState.h"
#include "IDriveCommandSink.h"






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
////////////////////////////////////////////////////////////////////////////////

class DriveWidgetController
{
public:
    enum class SyncAction
    {
        DoorOpen,
        DoorClose,
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
    std::vector<DriveSyncEvent>  m_syncEvents;
    uint64_t                     m_nextSyncEventId = 1;
};
