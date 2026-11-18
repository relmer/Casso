#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Ui/Scene/DeskDrivePlan.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMenuDrive
//
//  The drive a Storage menu command for drive 1 to drive 4 acts on: the
//  //c Plus's built-in 3.5-inch drive, a Disk II drive, or none.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskMenuDrive
{
    bool  isBuiltIn35 = false;
    int   diskIiDrive = -1;

    bool  IsDiskIi () const { return diskIiDrive >= 0; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMenuRule
//
//  The //c Plus boots from its built-in 3.5-inch drive, and its 5.25-inch
//  drives are external units on the disk port. Its drive 1 commands act on
//  the built-in drive, and its drive 2 and drive 3 commands on the first and
//  second 5.25-inch drives, when they are attached. On every other machine,
//  drive N is slot 6's Disk II drive N: drives 1 and 2 on an Apple II, and up
//  to four on the Apple ///.
//
////////////////////////////////////////////////////////////////////////////////

class DiskMenuRule
{
public:
    static bool           HasBuiltIn35Drive (const MachineConfig & config);
    static DiskMenuDrive  Resolve           (int menuDrive, bool hasBuiltIn35, int portFloppies, int diskIiDrives = 2);
    static DeskDrive      GetBuiltIn35Drive ()                                                    { return DeskDrive { DeskDriveModel::Apple35Plus, 0, 0 }; }
};
