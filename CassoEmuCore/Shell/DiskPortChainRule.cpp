#include "Pch.h"

#include "Shell/DiskPortChainRule.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Machines/MachineDefinitions.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortChainRule::IsOffered
//
//  The machines whose disk port takes Apple 3.5 Drives: the //c Plus.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskPortChainRule::IsOffered (const MachineConfig & config)
{
    const MachineDefinition *  definition = MachineDefinitions::Find (config.machineId);



    return (definition != nullptr && definition->diskPortApple35Count > 0);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortChainRule::CountDrives
//
////////////////////////////////////////////////////////////////////////////////

int DiskPortChainRule::CountDrives (const MachineConfig & config, const std::string & device)
{
    int  count = 0;



    for (const PortConfig & port : config.ports)
    {
        for (const PortConfig & entry : port.chain)
        {
            count += (entry.device == device) ? 1 : 0;
        }
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortChainRule::GetDriveLimit
//
////////////////////////////////////////////////////////////////////////////////

int DiskPortChainRule::GetDriveLimit (const MachineConfig & config, const std::string & device)
{
    const MachineDefinition *  definition = MachineDefinitions::Find (config.machineId);
    int                        limit      = 0;



    if (definition != nullptr && device == kpszApple35DriveDevice)
    {
        limit = definition->diskPortApple35Count;
    }
    else if (definition != nullptr && device == kpszDiskIicDevice)
    {
        limit = definition->diskPortFloppyCount;
    }

    return limit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortChainRule::CanPlugIn
//
////////////////////////////////////////////////////////////////////////////////

bool DiskPortChainRule::CanPlugIn (const MachineConfig & config, const std::string & device)
{
    size_t  total = 0;



    for (const PortConfig & port : config.ports)
    {
        total += port.chain.size();
    }

    return total < MachineConfigLoader::kMaxChainDrives && CountDrives (config, device) < GetDriveLimit (config, device);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortChainRule::CanSwap
//
//  A 3.5-inch drive swaps for a 5.25-inch drive and the reverse, while the
//  chain has room for one more of the other kind.
//
////////////////////////////////////////////////////////////////////////////////

bool DiskPortChainRule::CanSwap (const MachineConfig & config, const std::string & device)
{
    std::string  other = GetSwapDevice (device);



    return !other.empty() && CountDrives (config, other) < GetDriveLimit (config, other);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortChainRule::GetSwapDevice
//
////////////////////////////////////////////////////////////////////////////////

std::string DiskPortChainRule::GetSwapDevice (const std::string & device)
{
    std::string  other;



    if (device == kpszApple35DriveDevice)
    {
        other = kpszDiskIicDevice;
    }
    else if (device == kpszDiskIicDevice)
    {
        other = kpszApple35DriveDevice;
    }

    return other;
}
