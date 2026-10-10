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
//  ShellInspectorHost::PublishStatus
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorHost::PublishStatus()
{
    m_publisher.Publish (m_machine.GetRefs().diskController, *this);
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::GetImage
//
//  The disk in a slot 6 drive, as the CPU thread holds it, with its file's
//  size and read-only attribute for the Image tab (FR-050); both are left at
//  zero and clear when the file cannot be read. The file is looked at once
//  per disk, not on every frame the head state is published.
//
////////////////////////////////////////////////////////////////////////////////

const DiskImage * ShellInspectorHost::GetImage (int drive, InspectorDiskIdentity & outIdentity)
{
    DiskImageStore &  store      = m_machine.GetDiskStore();
    const DiskImage * image      = nullptr;
    std::error_code   error;
    DWORD             attributes = INVALID_FILE_ATTRIBUTES;
    uint64_t          mediaId    = 0;



    if (drive >= 0 && drive < kDriveCount && store.IsMounted (kDiskSlot, drive))
    {
        image   = store.GetImage (kDiskSlot, drive);
        mediaId = store.GetMediaId (kDiskSlot, drive);

        if (m_identities[drive].mediaId != mediaId)
        {
            InspectorDiskIdentity &  identity = m_identities[drive];

            identity.mediaId    = mediaId;
            identity.fileName   = store.GetSourcePath (kDiskSlot, drive);
            identity.fileSize   = std::filesystem::file_size (std::filesystem::path (identity.fileName), error);
            identity.fileSize   = error ? 0 : identity.fileSize;
            attributes          = GetFileAttributesW (std::filesystem::path (identity.fileName).c_str());
            identity.isReadOnly = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) != 0;
        }

        outIdentity = m_identities[drive];
    }

    return image;
}