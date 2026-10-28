#pragma once

#include "Pch.h"

#include "Machines/Apple2/Common/DiskAccessLog.h"
#include "Shell/InspectorRequestQueue.h"

class Disk2Controller;





////////////////////////////////////////////////////////////////////////////////
//
//  HeadActivity / DriveHeadState
//
//  What one drive is doing at the end of a frame, for the inspector's head
//  marker, strip and Tracks tab (FR-062 to FR-068): the motor and head, the
//  record under the head and the angle it has turned to, what the head is
//  doing, the write protection, and the guest's write and visit counts,
//  whose generation changes whenever any of them does.
//
////////////////////////////////////////////////////////////////////////////////

enum class HeadActivity
{
    Idle,
    Reading,
    Writing,
    WriteBlocked,
};


//  Where the guest read and wrote on each quarter track (FR-161).
struct DriveAccess
{
    DiskAccessLog::Spans  reads;
    DiskAccessLog::Spans  writes;
};


struct DriveHeadState
{
    bool                                                 hasController     = false;
    bool                                                 hasDisk           = false;
    bool                                                 isMotorOn         = false;
    bool                                                 isEnabled         = false;
    bool                                                 isTurning         = false;
    uint64_t                                             mediaId           = 0;
    int                                                  headQuarterTrack  = 0;
    int                                                  trackRecord       = -1;
    double                                               angle             = 0.0;
    uint64_t                                             readNibbles       = 0;
    uint64_t                                             writeNibbles      = 0;
    HeadActivity                                         activity          = HeadActivity::Idle;
    WriteProtectInfo                                     writeProtect;
    uint64_t                                             countsGeneration  = 0;
    uint64_t                                             saveCount         = 0;
    vector<uint64_t>                                     recordWriteCounts;
    std::array<uint32_t, DiskImage::kQuarterTrackCount>  visitCounts       = {};

    //  Shared with the state before while the log has not changed.
    std::shared_ptr<const DriveAccess>                   access;
    uint64_t                                             accessGeneration  = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  InspectorStatusPublisher
//
//  Builds each drive's state on the emulation thread at the end of a frame
//  and swaps it in whole, so the window reads a state that never changes
//  once published. Nothing is built while no window is shown (FR-070).
//
////////////////////////////////////////////////////////////////////////////////

class InspectorStatusPublisher
{
public:
    static constexpr int  kDriveCount = 2;

    //  Any thread.
    void                                   SetActive (bool isActive) { m_isActive = isActive; }
    std::shared_ptr<const DriveHeadState>  Get       (int drive) const;

    //  Emulation thread only.
    void  Publish (const Disk2Controller * controller, IInspectorDiskSource & source);

    static DriveHeadState  Build (const Disk2Controller * controller, IInspectorDiskSource & source, int drive, const DriveHeadState * previous);

private:
    static bool  IsSameCounts (const DriveHeadState & a, const DriveHeadState & b);

    std::atomic<bool>                                                 m_isActive = false;
    mutable std::mutex                                                m_lock;
    std::array<std::shared_ptr<const DriveHeadState>, kDriveCount>    m_states;
};
