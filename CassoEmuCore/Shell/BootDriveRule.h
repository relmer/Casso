#pragma once

#include "Pch.h"

#include "Core/MachineConfig.h"
#include "Ui/Scene/DeskDrivePlan.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootDrive
//
//  A drive the startup form can fill: the drive, and for a drive on a port
//  of the machine rather than of a card, that port's name.
//
////////////////////////////////////////////////////////////////////////////////

struct BootDrive
{
    DeskDrive    drive;
    std::string  portName;

    bool  IsDiskIi () const { return drive.model == DeskDriveModel::DiskII || drive.model == DeskDriveModel::Disk2c; }
};





////////////////////////////////////////////////////////////////////////////////
//
//  BootDriveRule
//
//  The drive the startup form opens for. The drives are taken in the order
//  the Apple II looks for a boot disk: a //c's built-in drive or a //c Plus's
//  internal 3.5-inch drive, then the enabled cards from slot 7 down to slot
//  1. A drive with an image ends the search, since the machine boots from
//  it. A hard disk with no image, or with the stock image, which is an empty
//  volume, is passed over, since the ROM goes on to the next slot. The first floppy drive with no image is the one returned.
//
//  Slot 6's Disk II has an image when drive 1 has a saved disk. Another
//  drive has one when its port in the configuration has an image.
//
////////////////////////////////////////////////////////////////////////////////

class BootDriveRule
{
public:
    static constexpr int           kDiskIiSlot        = 6;
    static constexpr const char *  kpszInternalPort   = "internal";

    static bool  TryFindDriveToFill (const MachineConfig & config, bool hasBuiltInDrive, bool hasSavedDisk1, BootDrive & outDrive);

    //  Puts the image on the drive's port in `config`, for a drive that is
    //  not slot 6's Disk II.
    static void  SetImage           (MachineConfig & config, const BootDrive & drive, const std::string & path);

private:
    static bool  IsFloppyDevice   (const std::string & device);
    static bool  TryGetCardFloppy (const std::string & card, const std::vector<PortConfig> & ports, int & outPort, DeskDriveModel & outModel);
    static bool  HasAnyImage      (const std::vector<PortConfig> & ports);
};
