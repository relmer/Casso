#include "Pch.h"

#include "Shell/ShellInspectorHost.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::ShellInspectorHost
//
////////////////////////////////////////////////////////////////////////////////

ShellInspectorHost::ShellInspectorHost (CpuManager & cpuManager, MachineHost & machine) :
    m_cpuManager (cpuManager),
    m_machine    (machine)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::PostInspectorRequest
//
////////////////////////////////////////////////////////////////////////////////

uint64_t ShellInspectorHost::PostInspectorRequest (const InspectorRequest & request)
{
    uint64_t  id = m_queue.Post (request);



    m_cpuManager.PostCommand (IDM_DISK_INSPECTOR_SERVICE, "");

    return id;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::TakeInspectorReplies
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorHost::TakeInspectorReplies (vector<InspectorReply> & outReplies)
{
    m_queue.TakeReplies (outReplies);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::Service
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorHost::Service()
{
    m_queue.Drain (*this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::GetImage
//
//  The disk in a slot 6 drive, as the CPU thread holds it.
//
////////////////////////////////////////////////////////////////////////////////

const DiskImage * ShellInspectorHost::GetImage (int drive, InspectorDiskIdentity & outIdentity)
{
    DiskImageStore &  store = m_machine.GetDiskStore();
    const DiskImage * image = nullptr;



    if (drive >= 0 && drive < kDriveCount && store.IsMounted (kDiskSlot, drive))
    {
        image                  = store.GetImage (kDiskSlot, drive);
        outIdentity.mediaId    = store.GetMediaId (kDiskSlot, drive);
        outIdentity.fileName   = store.GetSourcePath (kDiskSlot, drive);
        outIdentity.isReadOnly = (image != nullptr) && image->IsWriteProtected();
    }

    return image;
}
