#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskPortChainRule
//
//  The //c Plus's disk port, whose drives are attached from the Disk menu:
//  whether the machine offers that, how many drives of `device` are on the
//  port's chain, and how many it takes. The port takes two drives in all, so
//  a drive of `device` fits while the chain has fewer than two and fewer
//  than the limit of its kind.
//
////////////////////////////////////////////////////////////////////////////////

class DiskPortChainRule
{
public:
    static bool  IsOffered     (const MachineConfig & config);
    static int   CountDrives   (const MachineConfig & config, const std::string & device);
    static int   GetDriveLimit (const MachineConfig & config, const std::string & device);
    static bool  CanPlugIn     (const MachineConfig & config, const std::string & device);
    static bool  CanSwap       (const MachineConfig & config, const std::string & device);

    // The kind a drive of `device` swaps for: a 3.5-inch drive for a
    // 5.25-inch drive and the reverse. Empty for any other device.
    static std::string  GetSwapDevice (const std::string & device);
};
