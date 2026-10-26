#include "Pch.h"

#include "Shell/MachineChromeFacts.h"
#include "Core/TextEncoding.h"
#include "Machines/MachineDefinition.h"
#include "Machines/MachineDefinitions.h"
#include "Shell/MachineHost.h"





////////////////////////////////////////////////////////////////////////////////
//
//  MachineChromeFacts::Capture
//
////////////////////////////////////////////////////////////////////////////////

MachineChromeFacts MachineChromeFacts::Capture (const MachineConfig & config, const MachineRefs & refs, bool externalDriveConnected)
{
    const MachineDefinition *  definition = MachineDefinitions::Find (config.machineId);
    MachineChromeFacts         facts;



    facts.hasDiskController      = refs.diskController != nullptr;
    facts.hasCaseSwitches        = definition != nullptr && definition->hasCaseSwitches;
    facts.hasBuiltInDrive        = definition != nullptr && definition->hasBuiltInDrive;
    facts.hasBankedSystemRom     = config.systemRom.romBankSize != 0;
    facts.isSlotless             = config.slots.empty();
    facts.attachedDiskIiDrives   = config.AttachedDiskIiDriveCount();
    facts.externalDriveConnected = externalDriveConnected;
    facts.displayName            = TextEncoding::NarrowToWide (config.name);

    return facts;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineChromeFacts::TryCapture
//
////////////////////////////////////////////////////////////////////////////////

bool MachineChromeFacts::TryCapture (MachineHost & host, const bool & externalDriveConnected, MachineChromeFacts & facts)
{
    std::shared_lock<std::shared_mutex>  lock (host.GetLifetimeLock(), std::try_to_lock);
    bool                                 isLocked = lock.owns_lock();



    if (isLocked)
    {
        facts = Capture (host.GetConfig(), host.GetRefs(), externalDriveConnected);
    }

    return isLocked;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineChromeFacts::ShouldShowExternalDrive
//
//  The //c's second drive is an optional external unit that plugs into the
//  disk port, so it appears only when the user has marked it connected
//  (Hardware tab toggle -> $cassoUiPrefs.externalDriveConnected). The //c is
//  the only machine with a banked system ROM, so romBankSize is the
//  discriminator -- the same signal that gates the built-in IWM drive.
//
//  Everywhere else the second drive is whatever is attached to the Disk ][
//  card's second connector. The CARD having two connectors is not the same
//  claim as both of them having a drive on the end, and this is the question
//  the 2D widgets and the desk scene both ask, so answering it from the
//  config is what keeps them agreeing.
//
////////////////////////////////////////////////////////////////////////////////

bool MachineChromeFacts::ShouldShowExternalDrive() const
{
    return hasBankedSystemRom ? externalDriveConnected
                              : attachedDiskIiDrives >= kDiskIiPortCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  MachineChromeFacts::GetDeskSceneDriveCount
//
//  The same gates the 2D widgets use: no Disk II controller means no drives
//  at all; a //c with the external drive disconnected shows only the
//  internal one.
//
//  The controller check comes first and is separate from the config. A
//  machine can declare drive ports it cannot use -- the //c builds its IWM in
//  code from a banked-ROM test rather than from a slot -- so "is there a
//  controller" and "what is attached to it" are two questions, and answering
//  the second alone would put drives on a machine with no controller to run
//  them.
//
//  The //c's drives are not carded -- one is soldered in and the second is on
//  the back-panel disk port -- so its slot list indicates nothing about them,
//  and the internal drive is always there. A card with every port empty
//  reports zero, which is the point of being able to detach a drive at all.
//
////////////////////////////////////////////////////////////////////////////////

int MachineChromeFacts::GetDeskSceneDriveCount() const
{
    int  count = 0;



    if (!hasDiskController)
    {
        count = 0;
    }
    else if (isSlotless)
    {
        count = ShouldShowExternalDrive() ? 2 : 1;
    }
    else
    {
        count = attachedDiskIiDrives;
    }

    return count;
}
