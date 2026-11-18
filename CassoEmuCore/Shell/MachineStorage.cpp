#include "Pch.h"

#include "Shell/MachineStorage.h"
#include "Core/MachineConfig.h"
#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/BlockDevice/BlockDeviceMedia.h"
#include "Devices/BlockDevice/ProFileCard.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"
#include "Devices/Disk/DiskImageStore.h"
#include "Devices/Floppy35/Apple2cPlusDiskPort.h"
#include "Devices/Floppy35/Apple2cSmartPort.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"
#include "Devices/Floppy35/LironCard.h"
#include "Devices/Floppy35/SmartPortHost.h"
#include "Devices/Floppy35/Sony35Drive.h"
#include "Devices/Floppy35/UniDisk35Drive.h"
#include "Machines/Apple2/Common/Disk2Controller.h"
#include "Seams/Win32DiskFileIo.h"
#include "Shell/DiskManager.h"
#include "Shell/MachineParts.h"
#include "Shell/StorageCardWiring.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::MachineStorage
//
////////////////////////////////////////////////////////////////////////////////

MachineStorage::MachineStorage (MachineParts & parts, MachineConfig & config, DiskImageStore & diskStore) :
    m_parts     (parts),
    m_config    (config),
    m_diskStore (diskStore)
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::~MachineStorage
//
//  Out of line, where IDiskFileIo is complete.
//
////////////////////////////////////////////////////////////////////////////////

MachineStorage::~MachineStorage()
{
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::SetBlockMediaFileIo
//
//  Null restores the host's file system and its write-protect probe.
//
////////////////////////////////////////////////////////////////////////////////

void MachineStorage::SetBlockMediaFileIo (IDiskFileIo * fileIo, WriteProtectProbe probe)
{
    m_blockFileIo = fileIo;
    m_blockProbe  = std::move (probe);
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::GetBlockMediaFileIo
//
////////////////////////////////////////////////////////////////////////////////

IDiskFileIo & MachineStorage::GetBlockMediaFileIo()
{
    if (m_blockFileIo != nullptr)
    {
        return *m_blockFileIo;
    }

    if (m_hostBlockFileIo == nullptr)
    {
        m_hostBlockFileIo = std::make_unique<Win32DiskFileIo>();
    }

    return *m_hostBlockFileIo;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::ProbeBlockMedia
//
//  The host file's read-only attribute and write permission, for the host's
//  file system; nothing for an in-memory one without a probe.
//
////////////////////////////////////////////////////////////////////////////////

WriteProtectInfo MachineStorage::ProbeBlockMedia (const std::string & path) const
{
    WriteProtectInfo  info;



    if (m_blockProbe)
    {
        return m_blockProbe (path);
    }

    if (m_blockFileIo == nullptr)
    {
        DiskManager::ProbeFileWritability (path, info.readOnlyFile, info.noPermission);
    }

    return info;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::FindFloppy35
//
//  The //c Plus's drive 0 is its internal drive, then its external drives,
//  then the chain in order.
//
////////////////////////////////////////////////////////////////////////////////

Sony35Drive * MachineStorage::FindFloppy35 (int slot, int drive)
{
    MachineRefs  & refs     = m_parts.refs;
    Sony35Drive  * found    = nullptr;
    size_t         index    = (size_t) drive;
    size_t         external = 0;



    if (slot == 0 && refs.apple2cPlusDiskPort != nullptr)
    {
        Apple2cPlusDiskPort  & port = *refs.apple2cPlusDiskPort;



        external = (size_t) port.GetExternalCount();

        if (drive == 0)
        {
            found = &port.GetInternalDrive();
        }
        else if (index <= external)
        {
            found = &port.GetExternalDrive (drive - 1);
        }
        else if (index - 1 - external < port.GetPort().GetDriveCount())
        {
            found = &port.GetPort().GetDrive (index - 1 - external).GetMechanism();
        }
    }
    else if (slot == 0 && refs.apple2cSmartPort != nullptr && index < refs.apple2cSmartPort->GetPort().GetDriveCount())
    {
        found = &refs.apple2cSmartPort->GetPort().GetDrive (index).GetMechanism();
    }

    for (LironCard * card : refs.lironCards)
    {
        if (card->GetSlot() == slot && index < card->GetDriveCount())
        {
            found = &card->GetDrive (index).GetMechanism();
        }
    }

    for (Apple35ControllerCard * card : refs.apple35Cards)
    {
        if (card->GetSlot() == slot && (drive == 0 || drive == 1))
        {
            found = &card->GetCardMemory().GetDrive (drive);
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::InsertFloppy35
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStorage::InsertFloppy35 (int slot, int drive, const std::string & path, std::wstring & outError)
{
    HRESULT        hr     = S_OK;
    Sony35Drive  * target = FindFloppy35 (slot, drive);



    CBREx (target != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = target->Insert (GetBlockMediaFileIo(), path, ProbeBlockMedia (path), outError);
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::EjectFloppy35
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStorage::EjectFloppy35 (int slot, int drive)
{
    HRESULT        hr     = S_OK;
    Sony35Drive  * target = FindFloppy35 (slot, drive);



    CBREx (target != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    hr = target->Eject();
    CHR (hr);

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::AttachHardDisk
//
//  The image before is attached again when the new one fails, so a failed
//  change is no change.
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStorage::AttachHardDisk (int slot, int drive, const std::string & path, std::wstring & outError)
{
    HRESULT                  hr         = S_OK;
    HRESULT                  hrPrevious = S_OK;
    BlockDeviceMedia       * media      = nullptr;
    PortConfig             * port       = nullptr;
    const BlockDriveSpec   * spec       = nullptr;
    uint32_t                 maxBlocks  = 0;
    std::string              previous;
    std::wstring             ignored;



    for (AppleScsiCard * card : m_parts.refs.scsiCards)
    {
        if (card->GetSlot() == slot && drive >= 0 && drive < AppleScsiCard::kTargets)
        {
            media = &card->GetTarget (drive).GetMedia();
        }
    }

    for (ProFileCard * card : m_parts.refs.proFileCards)
    {
        if (card->GetDrive().GetSlot() == slot && drive == 0)
        {
            media     = &card->GetDrive().GetMedia();
            maxBlocks = card->GetDrive().GetCapacity();
        }
    }

    for (SlotConfig & entry : m_config.slots)
    {
        if (entry.slot == slot && drive >= 0 && (size_t) drive < entry.ports.size())
        {
            port = &entry.ports[(size_t) drive];
        }
    }

    CBREx (media != nullptr && port != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

    if (maxBlocks == 0)
    {
        spec = BlockDeviceCatalog::FindDrive (port->device);
        CBREx (spec != nullptr, HRESULT_FROM_WIN32 (ERROR_NOT_FOUND));

        maxBlocks = spec->maxBlocks;
    }

    previous = media->GetPath();

    hr = media->Attach (GetBlockMediaFileIo(), path, maxBlocks, ProbeBlockMedia (path), outError);

    if (SUCCEEDED (hr) && StorageCardWiring::RejectFloppyImage (*media, outError))
    {
        hr = HRESULT_FROM_WIN32 (ERROR_INVALID_DATA);
    }

    if (FAILED (hr) && !previous.empty())
    {
        hrPrevious = media->Attach (GetBlockMediaFileIo(), previous, maxBlocks, ProbeBlockMedia (previous), ignored);
        IGNORE_RETURN_VALUE (hrPrevious, S_OK);
    }

    CHR (hr);

    port->image = path;

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::ConnectDiskToController
//
//  A null image, on an eject, selects the drive's empty disk. Only slot 6
//  has a controller in the machine's references.
//
////////////////////////////////////////////////////////////////////////////////

void MachineStorage::ConnectDiskToController (int slot, int drive)
{
    Disk2Controller  * controller = m_parts.refs.diskController;



    if (slot != kDiskControllerSlot || controller == nullptr)
    {
        return;
    }

    controller->SetExternalDisk (drive, m_diskStore.GetImage (slot, drive));
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::BindDiskDrives
//
//  Each drive of the Disk II the refs have reads the disk in its bay, or its
//  empty disk when the bay is empty.
//
////////////////////////////////////////////////////////////////////////////////

void MachineStorage::BindDiskDrives()
{
    int  drive = 0;



    for (drive = 0; drive < DiskImageStore::kDriveCount; drive++)
    {
        ConnectDiskToController (kDiskControllerSlot, drive);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::GetReplayMedia
//
//  Each card's or port's drives, in the order the builder built them.
//
////////////////////////////////////////////////////////////////////////////////

void MachineStorage::GetReplayMedia (std::vector<IReplayMedia *> & outDrives)
{
    outDrives.clear();

    for (IReplayMediaSource * source : m_parts.refs.replayMediaSources)
    {
        source->GetReplayMedia (outDrives);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::CopyReplayMedia
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStorage::CopyReplayMedia (std::vector<ReplayBlockMedia> & outCopies)
{
    HRESULT                      hr = S_OK;
    std::vector<IReplayMedia *>  drives;
    size_t                       i  = 0;



    GetReplayMedia (drives);

    outCopies.assign (drives.size(), ReplayBlockMedia {});

    for (i = 0; i < drives.size(); i++)
    {
        hr = drives[i]->CopyForReplay (outCopies[i]);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineStorage::MountReplayMedia
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineStorage::MountReplayMedia (const std::vector<ReplayBlockMedia> & copies, std::vector<ReplayBlockMedia> & mounted)
{
    HRESULT                      hr           = S_OK;
    bool                         isSameDrives = false;
    bool                         isSame       = false;
    std::vector<IReplayMedia *>  drives;
    size_t                       i            = 0;



    GetReplayMedia (drives);

    isSameDrives = copies.size() == drives.size();
    CBREx (isSameDrives, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (mounted.size() != drives.size())
    {
        mounted.assign (drives.size(), ReplayBlockMedia {});
    }

    for (i = 0; i < drives.size(); i++)
    {
        isSame = copies[i].content == mounted[i].content && copies[i].path == mounted[i].path;

        if (!isSame)
        {
            mounted[i] = {};

            hr = drives[i]->MountReplayCopy (copies[i]);
            CHR (hr);

            mounted[i] = copies[i];
        }
    }

Error:
    return hr;
}
