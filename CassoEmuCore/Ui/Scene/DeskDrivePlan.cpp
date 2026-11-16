#include "Pch.h"

#include "Ui/Scene/DeskDrivePlan.h"

#include "Core/MachineConfig.h"
#include "Devices/BlockDevice/BlockDeviceCatalog.h"
#include "Devices/Disk/DriveImageRule.h"





////////////////////////////////////////////////////////////////////////////////
//
//  GetClass
//
////////////////////////////////////////////////////////////////////////////////

DeskDriveClass DeskDrivePlan::GetClass (DeskDriveModel model)
{
    return (model == DeskDriveModel::HardDisk20SC || model == DeskDriveModel::ProFile) ? DeskDriveClass::HardDisk
                                                                                         : DeskDriveClass::Floppy;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetReceivingKind
//
//  No default arm, so a model added without a kind is a compiler warning
//  and fails the sweep in DriveImageRuleTests.
//
////////////////////////////////////////////////////////////////////////////////

ReceivingDriveKind DeskDrivePlan::GetReceivingKind (DeskDriveModel model)
{
    switch (model)
    {
        case DeskDriveModel::DiskII:        return ReceivingDriveKind::Floppy525;
        case DeskDriveModel::Disk2c:        return ReceivingDriveKind::Floppy525;
        case DeskDriveModel::UniDisk35:     return ReceivingDriveKind::UniDisk35;
        case DeskDriveModel::Apple35Drive:  return ReceivingDriveKind::Apple35;
        case DeskDriveModel::Apple35Plus:   return ReceivingDriveKind::Apple35;
        case DeskDriveModel::HardDisk20SC:  return ReceivingDriveKind::HardDisk;
        case DeskDriveModel::ProFile:       return ReceivingDriveKind::HardDisk;
    }

    return ReceivingDriveKind::Floppy525;
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCaseWidthMm
//
//  The width of each desk model's case, from its CAD script.
//
////////////////////////////////////////////////////////////////////////////////

float DeskDrivePlan::GetCaseWidthMm (DeskDriveModel model)
{
    float  width = 0.0f;



    switch (model)
    {
        case DeskDriveModel::DiskII:       width = 156.0f; break;
        case DeskDriveModel::Disk2c:       width = 152.0f; break;
        case DeskDriveModel::UniDisk35:    width = 121.0f; break;
        case DeskDriveModel::Apple35Drive: width = 121.0f; break;
        case DeskDriveModel::Apple35Plus:  width = 121.0f; break;
        case DeskDriveModel::HardDisk20SC: width = 245.0f; break;
        case DeskDriveModel::ProFile:      width = 439.0f; break;
    }

    return width;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsOnDesk
//
////////////////////////////////////////////////////////////////////////////////

bool DeskDrivePlan::IsOnDesk (DeskClassPlace place, size_t count)
{
    constexpr size_t  kMostOnDeskByDefault = 2;



    return place == DeskClassPlace::Desk || (place == DeskClassPlace::Auto && count <= kMostOnDeskByDefault);
}





////////////////////////////////////////////////////////////////////////////////
//
//  Place
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DeskDrivePlacement> DeskDrivePlan::Place (const std::vector<DeskDrive> & drives,
                                                      DeskClassPlace                 floppies,
                                                      DeskClassPlace                 hardDisks,
                                                      float                          monitorWidthMm,
                                                      bool                           floppyPairBeside)
{
    std::vector<DeskDrivePlacement>  out;
    std::vector<DeskDrive>           floppy;
    std::vector<DeskDrive>           hard;
    bool                             floppyOnDesk = false;
    bool                             hardOnDesk   = false;
    bool                             wideHardDisk = false;
    bool                             diskIiPair   = false;



    for (const DeskDrive & drive : drives)
    {
        (GetClass (drive.model) == DeskDriveClass::Floppy ? floppy : hard).push_back (drive);
    }

    floppyOnDesk = !floppy.empty() && IsOnDesk (floppies,  floppy.size());
    hardOnDesk   = !hard.empty()   && IsOnDesk (hardDisks, hard.size());
    wideHardDisk = hardOnDesk && hard.size() == 1 && GetCaseWidthMm (hard[0].model) >= monitorWidthMm - kUnderMonitorSlackMm;

    if (wideHardDisk)
    {
        out.push_back ({ hard[0], DeskDriveSpot::UnderMonitor, 0 });
    }
    else if (hardOnDesk)
    {
        Stack (out, hard, DeskDriveSpot::LeftStack);
    }
    else
    {
        Stack (out, hard, DeskDriveSpot::DriveBar);
    }

    // The scene draws a pair under the monitor as its Disk II pair, so a
    // pair with another drive in it, such as the //c Plus's 3.5-inch drive
    // and a Disk IIc, goes beside the monitor.
    diskIiPair = floppy.size() == 2 && IsDiskII (floppy[0].model) && IsDiskII (floppy[1].model);

    if (!floppyOnDesk)
    {
        Stack (out, floppy, DeskDriveSpot::DriveBar);
    }
    else if (wideHardDisk || !diskIiPair || floppyPairBeside)
    {
        Stack (out, floppy, DeskDriveSpot::RightStack);
    }
    else
    {
        for (size_t i = 0; i < floppy.size(); i++)
        {
            out.push_back ({ floppy[i], DeskDriveSpot::UnderMonitor, (int) i });
        }
    }

    return out;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DeskDrivePlan::HasDiskIiPairUnderMonitor
//
////////////////////////////////////////////////////////////////////////////////

bool DeskDrivePlan::HasDiskIiPairUnderMonitor (const std::vector<DeskDrivePlacement> & plan)
{
    int  count = 0;



    for (const DeskDrivePlacement & placed : plan)
    {
        count += (placed.spot == DeskDriveSpot::UnderMonitor && IsDiskII (placed.drive.model)) ? 1 : 0;
    }

    return count == 2;
}





////////////////////////////////////////////////////////////////////////////////
//
//  IsDiskII
//
////////////////////////////////////////////////////////////////////////////////

bool DeskDrivePlan::IsDiskII (DeskDriveModel model)
{
    return model == DeskDriveModel::DiskII || model == DeskDriveModel::Disk2c;
}





////////////////////////////////////////////////////////////////////////////////
//
//  Stack
//
//  The widest case at level 0, the bottom; drives of one width are in the
//  order given.
//
////////////////////////////////////////////////////////////////////////////////

void DeskDrivePlan::Stack (std::vector<DeskDrivePlacement> & out, const std::vector<DeskDrive> & drives, DeskDriveSpot spot)
{
    std::vector<DeskDrive>  sorted = drives;



    std::stable_sort (sorted.begin(), sorted.end(), [] (const DeskDrive & a, const DeskDrive & b)
    {
        return GetCaseWidthMm (a.model) > GetCaseWidthMm (b.model);
    });

    for (size_t i = 0; i < sorted.size(); i++)
    {
        out.push_back ({ sorted[i], spot, (int) i });
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  GetCompactPlaces
//
////////////////////////////////////////////////////////////////////////////////

void DeskDrivePlan::GetCompactPlaces (const std::vector<DeskDrive> & drives, DeskClassPlace & outFloppies, DeskClassPlace & outHardDisks)
{
    bool  beyondDiskIi = std::any_of (drives.begin(), drives.end(), [] (const DeskDrive & drive) { return !IsDiskII (drive.model); });



    outFloppies  = beyondDiskIi ? DeskClassPlace::Bar : DeskClassPlace::Desk;
    outHardDisks = outFloppies;
}





////////////////////////////////////////////////////////////////////////////////
//
//  ParsePlace
//
////////////////////////////////////////////////////////////////////////////////

DeskClassPlace DeskDrivePlan::ParsePlace (const std::string & text)
{
    return (text == "desk") ? DeskClassPlace::Desk : (text == "bar") ? DeskClassPlace::Bar : DeskClassPlace::Auto;
}





////////////////////////////////////////////////////////////////////////////////
//
//  FormatPlace
//
////////////////////////////////////////////////////////////////////////////////

const char * DeskDrivePlan::FormatPlace (DeskClassPlace place)
{
    return (place == DeskClassPlace::Desk) ? "desk" : (place == DeskClassPlace::Bar) ? "bar" : "auto";
}





////////////////////////////////////////////////////////////////////////////////
//
//  Collect
//
////////////////////////////////////////////////////////////////////////////////

std::vector<DeskDrive> DeskDrivePlan::Collect (const MachineConfig & config, int diskIiCount, bool isC)
{
    std::vector<DeskDrive>  out;
    int                     chainFirst = 0;
    bool                    hasInner35 = false;



    for (const PortConfig & port : config.ports)
    {
        hasInner35 = hasInner35 || (port.name == "internal" && port.device == kpszApple35DriveDevice);
    }

    // The //c's drive 1 is built in; the //c Plus's built-in drive is its
    // 3.5-inch drive, so both of its 5.25-inch drives are on the port.
    for (int i = 0; i < diskIiCount; i++)
    {
        out.push_back ({ isC ? DeskDriveModel::Disk2c : DeskDriveModel::DiskII, 6, i, isC && i == 0 && !hasInner35, hasInner35 });
    }

    if (hasInner35)
    {
        out.push_back ({ DeskDriveModel::Apple35Plus, 0, 0 });
        chainFirst = 1;
    }

    for (const PortConfig & port : config.ports)
    {
        for (size_t i = 0; i < port.chain.size(); i++)
        {
            const std::string  & device = port.chain[i].device;
            int                  drive  = chainFirst + (int) i;



            if (device == kpszUniDisk35Device)
            {
                out.push_back ({ DeskDriveModel::UniDisk35, 0, drive });
            }
            else if (device == kpszApple35DriveDevice)
            {
                out.push_back ({ DeskDriveModel::Apple35Drive, 0, drive });
            }
        }
    }

    for (const SlotConfig & slot : config.slots)
    {
        if (!slot.enabled || BlockDeviceCatalog::FindCard (slot.device) == nullptr)
        {
            continue;
        }

        for (size_t i = 0; i < slot.ports.size(); i++)
        {
            const std::string  & device = slot.ports[i].device;



            if (device == kpszHd20scDevice)
            {
                out.push_back ({ DeskDriveModel::HardDisk20SC, slot.slot, (int) i });
            }
            else if (device == kpszProFile5Device || device == kpszProFile10Device)
            {
                out.push_back ({ DeskDriveModel::ProFile, slot.slot, (int) i });
            }
            else if (device == kpszUniDisk35Device)
            {
                out.push_back ({ DeskDriveModel::UniDisk35, slot.slot, (int) i });
            }
            else if (device == kpszApple35DriveDevice)
            {
                out.push_back ({ DeskDriveModel::Apple35Drive, slot.slot, (int) i });
            }
        }
    }

    return out;
}
