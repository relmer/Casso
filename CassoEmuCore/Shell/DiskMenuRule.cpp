#include "Pch.h"

#include "Shell/DiskMenuRule.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Shell/BootDriveRule.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMenuRule::HasBuiltIn35Drive
//
////////////////////////////////////////////////////////////////////////////////

bool DiskMenuRule::HasBuiltIn35Drive (const MachineConfig & config)
{
    for (const PortConfig & port : config.ports)
    {
        if (port.name == BootDriveRule::kpszInternalPort && port.device == kpszApple35DriveDevice)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskMenuRule::Resolve
//
//  `menuDrive` is 0 for the drive 1 commands and 1 for the drive 2
//  commands. `portFloppies` is the count of 5.25-inch drives on the disk
//  port.
//
////////////////////////////////////////////////////////////////////////////////

DiskMenuDrive DiskMenuRule::Resolve (int menuDrive, bool hasBuiltIn35, int portFloppies)
{
    DiskMenuDrive  target;



    if (!hasBuiltIn35)
    {
        target.diskIiDrive = menuDrive;
    }
    else if (menuDrive == 0)
    {
        target.isBuiltIn35 = true;
    }
    else if (portFloppies > 0)
    {
        target.diskIiDrive = 0;
    }

    return target;
}
