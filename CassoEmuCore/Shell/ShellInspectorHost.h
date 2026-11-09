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
    void      OnInspectorClosed    ()                                     override { if (m_onClosed) { m_onClosed(); } }

    //  UI thread: called when the user closes the window.
    void      SetOnClosed (std::function<void ()> onClosed) { m_onClosed = std::move (onClosed); }

    //  UI thread: the shell's functions for IsDriveAttached and
    //  HasOtherDiskController.
    void      SetDriveQueries (std::function<bool (int)> isAttached, std::function<bool ()> hasOther) { m_isAttached = std::move (isAttached); m_hasOther = std::move (hasOther); }

    bool      IsDriveAttached        (int drive) const override { return !m_isAttached || m_isAttached (drive); }
    bool      HasOtherDiskController () const          override { return m_hasOther && m_hasOther(); }

    std::shared_ptr<const DriveHeadState>  GetHeadState       (int drive) const override { return m_publisher.Get (drive); }
    void                                   SetHeadStateWanted (bool isWanted)  override { m_publisher.SetActive (isWanted); }

    //  CPU thread only: answers the window's requests, and at the end of each
    //  frame publishes each drive's head and counts.
    void      Service       ();
    void      PublishStatus ();

    //  CPU thread only, at the end of each frame: checks the applies that
    //  wait for a safe point again (FR-103).
    void      ServiceWaiting ();

private:
    const DiskImage *     GetImage         (int drive, InspectorDiskIdentity & outIdentity) override;
    DiskImage *           GetImageForEdit  (int drive) override;
    InspectorDriveMotion  GetMotion        (int drive) override;
    InspectorSaveResult   SaveDrive        (int drive) override;
    void                  SetMachinePaused (bool isPaused) override;
    vector<std::string>   GetMountedPaths  () override;

    CpuManager                                      & m_cpuManager;
    MachineHost                                     & m_machine;
    InspectorRequestQueue                             m_queue;
    InspectorStatusPublisher                          m_publisher;
    std::array<InspectorDiskIdentity, kDriveCount>    m_identities;
    std::function<void ()>                            m_onClosed;
    std::function<bool (int)>                         m_isAttached;
    std::function<bool ()>                            m_hasOther;
};
