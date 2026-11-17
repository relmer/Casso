#include "Pch.h"

#include "Devices/BlockDevice/AppleScsiCard.h"
#include "Devices/BlockDevice/ProFileCard.h"
#include "Devices/BlockDevice/ScsiHardDisk.h"
#include "Devices/Floppy35/Apple2cPlusDiskPort.h"
#include "Devices/Floppy35/Apple2cSmartPort.h"
#include "Devices/Floppy35/Apple35ControllerCard.h"
#include "Devices/Floppy35/LironCard.h"
#include "Devices/Floppy35/SmartPortHost.h"
#include "Devices/Floppy35/UniDisk35Drive.h"
#include "Shell/MachineRefs.h"
#include "Shell/MachineReplayMedia.h"





////////////////////////////////////////////////////////////////////////////////
//
//  AddChain
//
//  The mechanism of each UniDisk 3.5 on a SmartPort chain.
//
////////////////////////////////////////////////////////////////////////////////

static void AddChain (SmartPortHost & port, std::vector<IReplayMedia *> & outDrives)
{
    for (size_t i = 0; i < port.GetDriveCount(); i++)
    {
        outDrives.push_back (&port.GetDrive (i).GetMechanism());
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineReplayMedia::Get
//
////////////////////////////////////////////////////////////////////////////////

void MachineReplayMedia::Get (MachineRefs & refs, std::vector<IReplayMedia *> & outDrives)
{
    outDrives.clear();

    for (AppleScsiCard * card : refs.scsiCards)
    {
        for (int id = 0; id < AppleScsiCard::kTargets; id++)
        {
            outDrives.push_back (&card->GetTarget (id).GetMedia());
        }
    }

    for (ProFileCard * card : refs.proFileCards)
    {
        outDrives.push_back (&card->GetDrive().GetMedia());
    }

    for (Apple35ControllerCard * card : refs.apple35Cards)
    {
        for (int drive = 0; drive < Apple35CardMemory::kDrives; drive++)
        {
            if (card->GetCardMemory().HasDrive (drive))
            {
                outDrives.push_back (&card->GetCardMemory().GetDrive (drive));
            }
        }
    }

    for (LironCard * card : refs.lironCards)
    {
        for (size_t i = 0; i < card->GetDriveCount(); i++)
        {
            outDrives.push_back (&card->GetDrive (i).GetMechanism());
        }
    }

    if (refs.apple2cSmartPort != nullptr)
    {
        AddChain (refs.apple2cSmartPort->GetPort(), outDrives);
    }

    if (refs.apple2cPlusDiskPort != nullptr)
    {
        outDrives.push_back (&refs.apple2cPlusDiskPort->GetInternalDrive());

        for (int drive = 0; drive < Apple2cPlusDiskPort::kMaxExternal; drive++)
        {
            outDrives.push_back (&refs.apple2cPlusDiskPort->GetExternalDrive (drive));
        }

        AddChain (refs.apple2cPlusDiskPort->GetPort(), outDrives);
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineReplayMedia::Copy
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineReplayMedia::Copy (MachineRefs & refs, std::vector<ReplayBlockMedia> & outCopies)
{
    HRESULT                       hr = S_OK;
    std::vector<IReplayMedia *>   drives;



    Get (refs, drives);

    outCopies.assign (drives.size(), ReplayBlockMedia {});

    for (size_t i = 0; i < drives.size(); i++)
    {
        hr = drives[i]->CopyForReplay (outCopies[i]);
        CHR (hr);
    }

Error:
    return hr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineReplayMedia::Mount
//
////////////////////////////////////////////////////////////////////////////////

HRESULT MachineReplayMedia::Mount (MachineRefs & refs, const std::vector<ReplayBlockMedia> & copies, std::vector<ReplayBlockMedia> & mounted)
{
    HRESULT                      hr           = S_OK;
    bool                         isSameDrives = false;
    bool                         isSame       = false;
    std::vector<IReplayMedia *>  drives;



    Get (refs, drives);

    isSameDrives = copies.size() == drives.size();
    CBREx (isSameDrives, HRESULT_FROM_WIN32 (ERROR_INVALID_DATA));

    if (mounted.size() != drives.size())
    {
        mounted.assign (drives.size(), ReplayBlockMedia {});
    }

    for (size_t i = 0; i < drives.size(); i++)
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
