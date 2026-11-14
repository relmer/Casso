#include "Pch.h"

#include "Devices/BlockDevice/BlockDeviceCatalog.h"




static constexpr const char *  s_kpszScsiDrives[]    = { kpszHd20scDevice };
static constexpr const char *  s_kpszProFileDrives[] = { kpszProFile5Device, kpszProFile10Device };
static constexpr const char *  s_kpszLironDrives[]   = { kpszUniDisk35Device };
static constexpr const char *  s_kpszApple35Drives[] = { kpszApple35DriveDevice };

static constexpr const char *  s_kpszPlusAndIIe[]    = { "Apple2Plus", "Apple2e", "Apple2eEnhanced" };
static constexpr const char *  s_kpszIIeOnly[]       = { "Apple2e", "Apple2eEnhanced" };

static constexpr const char *  s_kpszMachineLabels[][2] =
{
    { "Apple2",          "Apple ][" },
    { "Apple2Plus",      "Apple ][+" },
    { "Apple2e",         "Apple //e" },
    { "Apple2eEnhanced", "Apple //e Enhanced" },
    { "Apple2c",         "Apple //c" },
    { "Apple2cPlus",     "Apple //c Plus" },
};

static const BlockCardSpec  s_kCards[] =
{
    { kpszAppleScsiCardDevice, "Apple II SCSI Card",             "Devices/AppleScsiCard", 16384, "341-0437-A.rom", 7, false, s_kpszScsiDrives,    s_kpszPlusAndIIe },
    { kpszProFileCardDevice,   "ProFile interface card",         "Devices/ProFileCard",    2048, "341-0271-A.rom", 1, false, s_kpszProFileDrives, s_kpszPlusAndIIe },
    { kpszLironCardDevice,     "UniDisk 3.5 interface card",     "Devices/LironCard",      4096, "Liron.rom",      4, true,  s_kpszLironDrives,   s_kpszPlusAndIIe },
    { kpszApple35CardDevice,   "Apple II 3.5 Disk Controller Card", "Devices/Apple35Card", 32768, "341-0438-A.rom", 2, false, s_kpszApple35Drives, s_kpszIIeOnly },
};

static const BlockDriveSpec  s_kDrives[] =
{
    { kpszHd20scDevice,       "Apple Hard Disk 20SC", true,  BlockDeviceCatalog::kProDosMaxBlocks },
    { kpszProFile5Device,     "ProFile 5 MB",         true,  9728 },
    { kpszProFile10Device,    "ProFile 10 MB",        true,  19456 },
    { kpszUniDisk35Device,    "UniDisk 3.5",          false, 1600, "Devices/UniDisk35", "UniDisk35.rom", 8192 },
    { kpszApple35DriveDevice, "Apple 3.5 Drive",      false, 1600 },
};





////////////////////////////////////////////////////////////////////////////////
//
//  GetCards
//
////////////////////////////////////////////////////////////////////////////////

std::span<const BlockCardSpec> BlockDeviceCatalog::GetCards()
{
    return s_kCards;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetDrives
//
////////////////////////////////////////////////////////////////////////////////

std::span<const BlockDriveSpec> BlockDeviceCatalog::GetDrives()
{
    return s_kDrives;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindCard
//
////////////////////////////////////////////////////////////////////////////////

const BlockCardSpec * BlockDeviceCatalog::FindCard (const std::string & device)
{
    for (const BlockCardSpec & card : s_kCards)
    {
        if (device == card.device)
        {
            return &card;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FindDrive
//
////////////////////////////////////////////////////////////////////////////////

const BlockDriveSpec * BlockDeviceCatalog::FindDrive (const std::string & device)
{
    for (const BlockDriveSpec & drive : s_kDrives)
    {
        if (device == drive.device)
        {
            return &drive;
        }
    }

    return nullptr;
}





////////////////////////////////////////////////////////////////////////////////
//
//  CanHost
//
//  A machine id Casso does not ship is a machine a user defined, which may have
//  any hardware, so it can host every card.
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceCatalog::CanHost (const BlockCardSpec & card, const std::string & machineId)
{
    bool  isKnown = GetMachineLabel (machineId) != nullptr;



    if (!isKnown)
    {
        return true;
    }

    for (const char * host : card.hosts)
    {
        if (machineId == host)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  AcceptsDrive
//
////////////////////////////////////////////////////////////////////////////////

bool BlockDeviceCatalog::AcceptsDrive (const BlockCardSpec & card, const std::string & drive)
{
    for (const char * accepted : card.drives)
    {
        if (drive == accepted)
        {
            return true;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetHostList
//
////////////////////////////////////////////////////////////////////////////////

std::string BlockDeviceCatalog::GetHostList (const BlockCardSpec & card)
{
    std::string  list;



    for (const char * host : card.hosts)
    {
        if (!list.empty())
        {
            list += ", ";
        }

        list += GetMachineLabel (host);
    }

    return list;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetMachineLabel
//
//  The display name of a machine Casso ships, or null for any other id.
//
////////////////////////////////////////////////////////////////////////////////

const char * BlockDeviceCatalog::GetMachineLabel (const std::string & machineId)
{
    for (const auto & row : s_kpszMachineLabels)
    {
        if (machineId == row[0])
        {
            return row[1];
        }
    }

    return nullptr;
}
