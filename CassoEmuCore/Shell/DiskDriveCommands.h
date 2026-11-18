#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImageStore.h"
#include "resource.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskDriveCommands
//
//  The commands posted to the CPU thread for each Disk II drive: insert,
//  eject, the image's write-protect toggle and the user's write-protect
//  preference. Drive 0 is drive 1. An Apple II machine has drives 0 and 1;
//  the Apple /// has up to four.
//
//  The menu commands with the same ids act on a menu drive instead, which
//  DiskMenuRule resolves. A posted command never passes through the menu.
//
////////////////////////////////////////////////////////////////////////////////

class DiskDriveCommands
{
public:
    enum class Kind
    {
        Insert,
        Eject,
        ImageWriteProtect,
        UserWriteProtect,

        // Menu commands only, never posted.
        Salvage,
        Inspect,
        Thumbnail,
    };

    static constexpr int  kDriveCount = DiskImageStore::kDriveCount;

    // The command for a drive, or 0 for a drive out of range.
    static WORD  GetCommand (Kind kind, int drive);

    // The drive and kind a posted command addresses. Returns false for a
    // command that is not one of these.
    static bool  TryGetDrive (WORD command, Kind & outKind, int & outDrive);

    // The menu drive, 0 to 3, a Disk menu row acts on, or -1 for a command
    // that is not a Disk menu row.
    static int   GetMenuDrive (WORD command);
};
