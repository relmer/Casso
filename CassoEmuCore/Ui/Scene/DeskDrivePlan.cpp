#include "Pch.h"

#include "Ui/Scene/DeskDrivePlan.h"





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
        case DeskDriveModel::UniDisk35:    width = 175.0f; break;
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
                                                      float                          monitorWidthMm)
{
    std::vector<DeskDrivePlacement>  out;
    std::vector<DeskDrive>           floppy;
    std::vector<DeskDrive>           hard;
    bool                             floppyOnDesk = false;
    bool                             hardOnDesk   = false;
    bool                             wideHardDisk = false;



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

    if (!floppyOnDesk)
    {
        Stack (out, floppy, DeskDriveSpot::DriveBar);
    }
    else if (wideHardDisk || floppy.size() > 2)
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
