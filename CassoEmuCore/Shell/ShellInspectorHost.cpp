#include "Pch.h"

#include "Shell/ShellInspectorHost.h"
#include "Core/TextEncoding.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
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
//  The disk in a slot 6 drive, as the CPU thread has it, with its file's
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





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::ServiceWaiting
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorHost::ServiceWaiting()
{
    if (m_queue.HasWaiting())
    {
        m_queue.Drain (*this);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::GetImageForEdit
//
////////////////////////////////////////////////////////////////////////////////

DiskImage * ShellInspectorHost::GetImageForEdit (int drive)
{
    DiskImageStore &  store = m_machine.GetDiskStore();



    return (drive >= 0 && drive < kDriveCount && store.IsMounted (kDiskSlot, drive)) ? store.GetImage (kDiskSlot, drive) : nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::GetMotion
//
//  A drive turns while the motor runs, its spin-down included, and it is
//  the selected drive; it is writing while the write mode is on.
//
////////////////////////////////////////////////////////////////////////////////

InspectorDriveMotion ShellInspectorHost::GetMotion (int drive)
{
    Disk2Controller *     controller = m_machine.GetRefs().diskController;
    InspectorDriveMotion  motion;



    motion.isPaused = m_cpuManager.IsPaused();

    if (controller != nullptr && drive >= 0 && drive < kDriveCount)
    {
        const Disk2NibbleEngine &  engine = controller->GetEngine (drive);

        motion.isTurning    = controller->IsMotorOn() && controller->GetActiveDrive() == drive;
        motion.isWriting    = motion.isTurning && controller->IsQ7();
        motion.slot         = engine.GetSlot();
        motion.angle        = engine.GetAngle();
        motion.writeNibbles = engine.GetWriteNibbles();
    }

    return motion;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::SaveDrive
//
//  Casso's usual save, with its checks (FR-104). When the file changed
//  outside Casso, the save goes to a preserved copy and the drive moves to
//  it, so the drive's path afterward differs from the one before.
//
////////////////////////////////////////////////////////////////////////////////

InspectorSaveResult ShellInspectorHost::SaveDrive (int drive)
{
    DiskImageStore &     store  = m_machine.GetDiskStore();
    std::string          before = store.GetSourcePath (kDiskSlot, drive);
    InspectorSaveResult  result;



    result.hr   = store.Flush (kDiskSlot, drive);
    result.path = store.GetSourcePath (kDiskSlot, drive);

    if (FAILED (result.hr))
    {
        result.reason = TextEncoding::NarrowToWide (std::system_category().message (result.hr));
    }
    else if (store.GetImage (kDiskSlot, drive) != nullptr && store.GetImage (kDiskSlot, drive)->IsDirty())
    {
        //  A save the store withheld, such as one waiting on the answer to a
        //  question about the file, has not been written to the file.
        result.hr     = STG_E_NOTCURRENT;
        result.reason = L"Casso is not saving this disk until the question about its file is answered";
    }

    result.isPreservedCopy = SUCCEEDED (result.hr) && result.path != before;

    return result;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ShellInspectorHost::SetMachinePaused
//
////////////////////////////////////////////////////////////////////////////////

void ShellInspectorHost::SetMachinePaused (bool isPaused)
{
    m_cpuManager.SetPaused (isPaused);
}