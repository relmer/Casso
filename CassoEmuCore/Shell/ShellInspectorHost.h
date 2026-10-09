#pragma once

#include "Pch.h"

#include "Shell/CpuManager.h"
#include "Shell/InspectorRequestQueue.h"
#include "Shell/MachineHost.h"
#include "Ui/DiskInspector/IDiskInspectorHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost
//
//  Casso's side of the disk inspector. A request from the window is queued
//  and a command posted to the CPU thread, which wakes it even while the
//  machine is paused; there Service answers every queued request from the
//  disk store's slot 6 drives. The window takes the replies on its own
//  frame.
//
////////////////////////////////////////////////////////////////////////////////

class ShellInspectorHost : public IDiskInspectorHost,
                           private IInspectorDiskSource
{
public:
    static constexpr int  kDiskSlot   = 6;
    static constexpr int  kDriveCount = 2;

    ShellInspectorHost (CpuManager & cpuManager, MachineHost & machine);

    uint64_t  PostInspectorRequest (const InspectorRequest & request)   override;
    void      TakeInspectorReplies (vector<InspectorReply> & outReplies) override;
    int       GetDriveCount        () const                               override { return kDriveCount; }
    void      OnInspectorClosed    ()                                     override {}

    //  CPU thread only.
    void      Service ();

private:
    const DiskImage *  GetImage (int drive, InspectorDiskIdentity & outIdentity) override;

    CpuManager &           m_cpuManager;
    MachineHost &          m_machine;
    InspectorRequestQueue  m_queue;
};
