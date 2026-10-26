#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Shell/MachineRefs.h"

class MachineHost;





////////////////////////////////////////////////////////////////////////////////
//
//  MachineChromeFacts
//
//  What the window's layout reads about the machine: whether there is a disk
//  controller, a switch panel or a built-in drive, how many drives are
//  attached, and the machine's name.
//
//  A COPY, TAKEN ONLY UNDER THE MACHINE'S LIFETIME LOCK. A machine switch
//  rewrites the config and the device references on the CPU thread under that
//  lock, so a layout that read them directly could walk a slot list while it
//  was being replaced. The UI thread reads this copy instead, and refreshes
//  it when the machine has changed.
//
////////////////////////////////////////////////////////////////////////////////

struct MachineChromeFacts
{
    bool          hasDiskController      = false;
    bool          hasCaseSwitches        = false;
    bool          hasBuiltInDrive        = false;
    bool          hasBankedSystemRom     = false;
    bool          isSlotless             = false;
    int           attachedDiskIiDrives   = 0;
    bool          externalDriveConnected = false;
    std::wstring  displayName;

    //  The facts of a machine whose config and references are stable.
    static MachineChromeFacts  Capture    (const MachineConfig & config, const MachineRefs & refs, bool externalDriveConnected);

    //  The facts of `host`'s machine, under its lifetime lock taken shared
    //  without waiting. When the lock is not available, `facts` is left as it
    //  was and the call returns false.
    static bool                TryCapture (MachineHost & host, const bool & externalDriveConnected, MachineChromeFacts & facts);

    //  Whether the machine's second drive is shown: on a machine with a
    //  banked system ROM, the //c, when its external drive is connected;
    //  elsewhere, when the Disk II card has a drive on each connector.
    bool  ShouldShowExternalDrive () const;

    //  How many drives the desk scene draws: none without a controller; the
    //  internal drive and the external one when shown on a slotless machine;
    //  otherwise the drives attached to the card.
    int   GetDeskSceneDriveCount  () const;
};
