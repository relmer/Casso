#include "Pch.h"

#include "Shell/BootDriveRule.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"





////////////////////////////////////////////////////////////////////////////////
//
//  BootDriveRule::TryFindDriveToFill
//
////////////////////////////////////////////////////////////////////////////////

bool BootDriveRule::TryFindDriveToFill (const MachineConfig & config, bool hasBuiltInDrive, bool hasSavedDisk1, BootDrive & outDrive)
{
    std::vector<const SlotConfig *>  slots;



    outDrive = BootDrive();

    if (hasBuiltInDrive)
    {
        outDrive.drive = DeskDrive { DeskDriveModel::Disk2c, kDiskIiSlot, 0, true };
        return !hasSavedDisk1;
    }

    for (const PortConfig & port : config.ports)
    {
        if (port.name == kpszInternalPort && port.device == kpszApple35DriveDevice)
        {
            outDrive.drive    = DeskDrive { DeskDriveModel::Apple35Plus, 0, 0 };
            outDrive.portName = port.name;
            return port.image.empty();
        }
    }

    for (const SlotConfig & slot : config.slots)
    {
        if (slot.enabled && !slot.device.empty())
        {
            slots.push_back (&slot);
        }
    }

    std::sort (slots.begin(), slots.end(), [] (const SlotConfig * a, const SlotConfig * b) { return a->slot > b->slot; });

    for (const SlotConfig * slot : slots)
    {
        int             port  = 0;
        DeskDriveModel  model = DeskDriveModel::DiskII;



        if (slot->device == kpszDiskIiDevice && slot->slot == kDiskIiSlot)
        {
            outDrive.drive = DeskDrive { DeskDriveModel::DiskII, kDiskIiSlot, 0 };
            return !hasSavedDisk1;
        }

        if (TryGetCardFloppy (slot->device, slot->ports, port, model))
        {
            outDrive.drive = DeskDrive { model, slot->slot, port };
            return slot->ports.size() <= (size_t) port || slot->ports[(size_t) port].image.empty();
        }

        //  A hard disk with an image boots; with none, the ROM goes on.
        if (BlockDeviceCatalog::FindCard (slot->device) != nullptr && HasAnyImage (slot->ports))
        {
            return false;
        }
    }

    outDrive = BootDrive();
    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootDriveRule::SetImage
//
////////////////////////////////////////////////////////////////////////////////

void BootDriveRule::SetImage (MachineConfig & config, const BootDrive & drive, const std::string & path)
{
    if (!drive.portName.empty())
    {
        for (PortConfig & port : config.ports)
        {
            if (port.name == drive.portName)
            {
                port.image = path;
            }
        }

        return;
    }

    for (SlotConfig & slot : config.slots)
    {
        if (slot.slot == drive.drive.slot && slot.ports.size() > (size_t) drive.drive.drive)
        {
            slot.ports[(size_t) drive.drive.drive].image = path;
        }
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootDriveRule::IsFloppyDevice
//
////////////////////////////////////////////////////////////////////////////////

bool BootDriveRule::IsFloppyDevice (const std::string & device)
{
    return device == kpszApple35DriveDevice || device == kpszUniDisk35Device;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootDriveRule::TryGetCardFloppy
//
//  The first 3.5-inch drive on an Apple 3.5 card or a UniDisk 3.5 card.
//
////////////////////////////////////////////////////////////////////////////////

bool BootDriveRule::TryGetCardFloppy (const std::string & card, const std::vector<PortConfig> & ports, int & outPort, DeskDriveModel & outModel)
{
    bool  found = false;



    outPort  = 0;
    outModel = (card == kpszLironCardDevice) ? DeskDriveModel::UniDisk35 : DeskDriveModel::Apple35Drive;

    if (card != kpszApple35CardDevice && card != kpszLironCardDevice)
    {
        return false;
    }

    for (size_t i = 0; i < ports.size() && !found; i++)
    {
        if (IsFloppyDevice (ports[i].device))
        {
            outPort  = (int) i;
            outModel = (ports[i].device == kpszUniDisk35Device) ? DeskDriveModel::UniDisk35 : DeskDriveModel::Apple35Drive;
            found    = true;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  BootDriveRule::HasAnyImage
//
//  The stock image is an empty ProDOS volume with no boot loader, which the
//  card's ROM passes over, so it doesn't count.
//
////////////////////////////////////////////////////////////////////////////////

bool BootDriveRule::HasAnyImage (const std::vector<PortConfig> & ports)
{
    for (const PortConfig & port : ports)
    {
        if (!port.image.empty() && port.image != kpszStockImage)
        {
            return true;
        }
    }

    return false;
}
