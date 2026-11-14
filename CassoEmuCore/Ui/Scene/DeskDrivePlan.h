#pragma once

#include "Pch.h"

struct MachineConfig;





//  The drive models the desk and the drive bar draw.
enum class DeskDriveModel
{
    DiskII,
    Disk2c,          // the //c's drive, built in or external
    UniDisk35,
    Apple35Drive,
    Apple35Plus,     // the //c Plus's built-in drive, in its colors
    HardDisk20SC,
    ProFile,
};

//  The two classes the desk places as a whole.
enum class DeskDriveClass
{
    Floppy,
    HardDisk,
};

//  Where a class goes, from the drives' right-click menu. Auto puts a class
//  of more than two drives on the drive bar and a smaller one on the desk.
enum class DeskClassPlace
{
    Auto,
    Desk,
    Bar,
};

//  Where one drive goes.
enum class DeskDriveSpot
{
    UnderMonitor,
    LeftStack,
    RightStack,
    DriveBar,
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeskDrive
//
//  One drive of the machine: its model and its address, the card's slot (0
//  for the //c disk port) and the drive on the card (a drive number, a SCSI
//  ID or a chain position).
//
////////////////////////////////////////////////////////////////////////////////

struct DeskDrive
{
    DeskDriveModel  model = DeskDriveModel::DiskII;
    int             slot  = 0;
    int             drive = 0;
};


struct DeskDrivePlacement
{
    DeskDrive      drive;
    DeskDriveSpot  spot  = DeskDriveSpot::DriveBar;

    //  In a stack and on the drive bar, 0 is the bottom. Under the monitor,
    //  0 is the left drive and 1 the right.
    int            level = 0;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DeskDrivePlan
//
//  Where each of a machine's drives goes, from the two classes' settings:
//
//    - A class of more than two drives goes on the drive bar unless it is
//      set to the desk; a class set to the drive bar goes there whatever
//      its size.
//    - On the desk, each class is one stack, the widest case at the bottom
//      and the narrowest on top.
//    - A single hard disk at least as wide as the monitor, less a few
//      millimeters, goes under it, and the floppies stack on the right.
//    - Otherwise two floppies sit side by side under the monitor, and the
//      hard disks stack on the left.
//    - A single floppy, or more than two, stacks on the right, the hard
//      disks on the left, and nothing is under the monitor.
//
////////////////////////////////////////////////////////////////////////////////

class DeskDrivePlan
{
public:
    static constexpr float  kUnderMonitorSlackMm = 10.0f;

    static DeskDriveClass  GetClass       (DeskDriveModel model);
    static float           GetCaseWidthMm (DeskDriveModel model);
    static bool            IsOnDesk       (DeskClassPlace place, size_t count);

    //  "auto", "desk" or "bar", as the global preferences store a class's
    //  place; anything else is Auto.
    static DeskClassPlace  ParsePlace     (const std::string & text);
    static const char *    FormatPlace    (DeskClassPlace place);

    //  The machine's drives: `diskIiCount` Disk II drives in slot 6 (the
    //  //c's built-in and external drives when `isC`), the //c Plus's built-in drive and the
    //  //c disk port's chain, and the drives on the enabled storage cards'
    //  ports.
    static std::vector<DeskDrive>  Collect (const MachineConfig & config, int diskIiCount, bool isC);

    static std::vector<DeskDrivePlacement>  Place (const std::vector<DeskDrive> & drives,
                                                   DeskClassPlace                 floppies,
                                                   DeskClassPlace                 hardDisks,
                                                   float                          monitorWidthMm);

private:
    static void  Stack (std::vector<DeskDrivePlacement> & out, const std::vector<DeskDrive> & drives, DeskDriveSpot spot);
};
