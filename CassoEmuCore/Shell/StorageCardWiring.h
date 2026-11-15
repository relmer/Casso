#pragma once

#include "Pch.h"

#include "Shell/MachineBuilder.h"



class BlockDeviceMedia;





////////////////////////////////////////////////////////////////////////////////
//
//  StorageCardWiring
//
//  The machine build's storage devices: the SCSI, ProFile, 3.5-inch and
//  UniDisk 3.5 cards in their slots, a SmartPort chain on the //c disk port,
//  the //c Plus's internal drive and MIG, and the drive sounds for each.
//  Factored out of MachineBuilder, which calls it at each step.
//
////////////////////////////////////////////////////////////////////////////////

class StorageCardWiring
{
public:
    StorageCardWiring (MachineBuilder & builder, MachineHost & host, MachineBuildServices & services);

    void  WireAppleScsiCard     (const SlotConfig & slot, AppleScsiCard & card);
    void  WireProFileCard       (const SlotConfig & slot, ProFileCard & card);
    void  WireApple35Card       (const SlotConfig & slot, Apple35ControllerCard & card);
    void  WireLironCard         (const SlotConfig & slot, LironCard & card);
    void  WireDiskPortChain     (Disk2Controller & controller);
    void  WireApple2cPlusDrives (Apple2cPlusDiskPort & port);
    void  WireMig               ();
    void  WireDeviceAudio       ();

private:
    void  AddChainDrives (const PortConfig & port, SmartPortHost & host);

    //  Detaches a floppy-size image from a hard disk. Returns true when it
    //  was one, with the reason in `outError`.
    static bool  RejectFloppyImage (BlockDeviceMedia & media, std::wstring & outError);

    MachineBuilder        & m_builder;
    MachineHost           & m_host;
    MachineBuildServices  & m_services;
};
