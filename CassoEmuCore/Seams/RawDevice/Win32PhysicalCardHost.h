#pragma once

#include "Pch.h"

#include "Devices/Disk/Partitions/PhysicalCards.h"

class ProxyProcess;





////////////////////////////////////////////////////////////////////////////////
//
//  Win32PhysicalCardHost
//
//  The PC's drives for PhysicalCards. A drive is listed from this process,
//  and opened through the storage card proxy, which is started elevated
//  for the first card opened; Windows raises the UAC prompt then, and once.
//  The proxy serves every later card, and exits when the pipe closes.
//  A proxy that has exited is started again for the next card.
//
////////////////////////////////////////////////////////////////////////////////

class Win32PhysicalCardHost : public IPhysicalCardHost
{
public:

    HRESULT  ListDrives (std::vector<PhysicalDrive> & outDrives) override;
    HRESULT  OpenDrive  (int number, HWND owner, PhysicalDrive & outDrive, std::unique_ptr<IBlockStore> & outStore) override;

    HRESULT  HasUnmountedPartition (int number, bool & outHasUnmounted) override;
    HRESULT  ListVolumes           (int number, std::vector<WindowsVolume> & outVolumes) override;
    HRESULT  InitializeDrive       (int number, HWND owner) override;

private:
    HRESULT  StartProxy (HWND owner);

    //  Shared with each card's store, so the proxy runs until this host and
    //  every store are destroyed.
    std::shared_ptr<ProxyProcess>  m_proxy;
};