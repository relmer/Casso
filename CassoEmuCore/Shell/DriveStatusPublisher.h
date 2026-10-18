#pragma once

#include "Pch.h"

#include "Devices/Disk/DriveStatus.h"


class Disk2Controller;
class DiskImageStore;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveStatusChange
//
//  What a publish changed: nothing, only the drives' activity, or a bay.
//
////////////////////////////////////////////////////////////////////////////////

enum class DriveStatusChange
{
    None,
    Activity,
    Bays,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DriveStatusPublisher
//
//  Publishes the drive status for the window. Publish runs on the thread that
//  has the disk store's token, and rebuilds the bay table only when the
//  store's status generation moved; Take may run on any thread and returns
//  the newest status, never null. The mutex guards only the published
//  pointer.
//
////////////////////////////////////////////////////////////////////////////////

class DriveStatusPublisher
{
public:
    DriveStatusPublisher ();

    DriveStatusChange                   Publish (const DiskImageStore  & store,
                                                 const Disk2Controller * controller,
                                                 bool                    isActivityDue);
    std::shared_ptr<const DriveStatus>  Take    () const;

private:
    static void  BuildBays      (const DiskImageStore & store, DriveStatus::BayTable & out);
    static void  SampleActivity (const Disk2Controller * controller, DriveStatus::ActivityTable & out);

    //  The publishing thread only.
    uint64_t                                      m_sequence      = 0;
    uint64_t                                      m_bayGeneration = 0;
    std::shared_ptr<const DriveStatus::BayTable>  m_bays;
    bool                                          m_hasController = false;
    DriveStatus::ActivityTable                    m_activity      {};

    mutable std::mutex                            m_mutex;
    std::shared_ptr<const DriveStatus>            m_published;
};