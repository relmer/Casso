#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PhysicalCards.h"





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost
//
//  The PC's drives for PhysicalCards. A drive is listed from this process
//  and opened through Win32RawDeviceHelper, which runs the program again
//  elevated; Windows raises the UAC prompt for each drive opened. Destroying
//  the store closes the pipe, and the elevated process then exits.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PhysicalCardHost : public IPhysicalCardHost
{
public:
    HRESULT  ListDrives (std::vector<PhysicalDrive> & outDrives) override;
    HRESULT  OpenDrive  (int number, HWND owner, PhysicalDrive & outDrive, std::unique_ptr<IBlockStore> & outStore) override;
};