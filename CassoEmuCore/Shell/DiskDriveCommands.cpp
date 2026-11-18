#include "Pch.h"

#include "Shell/DiskDriveCommands.h"





static constexpr WORD  s_kInsert[DiskDriveCommands::kDriveCount]     = { IDM_DISK_INSERT1,       IDM_DISK_INSERT2,       IDM_DISK_INSERT3,       IDM_DISK_INSERT4       };
static constexpr WORD  s_kEject[DiskDriveCommands::kDriveCount]      = { IDM_DISK_EJECT1,        IDM_DISK_EJECT2,        IDM_DISK_EJECT3,        IDM_DISK_EJECT4        };
static constexpr WORD  s_kWp[DiskDriveCommands::kDriveCount]         = { IDM_DISK_WP1,           IDM_DISK_WP2,           IDM_DISK_WP3,           IDM_DISK_WP4           };
static constexpr WORD  s_kUserWp[DiskDriveCommands::kDriveCount]     = { IDM_DISK_WRITEPROTECT1, IDM_DISK_WRITEPROTECT2, IDM_DISK_WRITEPROTECT3, IDM_DISK_WRITEPROTECT4 };
static constexpr WORD  s_kSalvage[DiskDriveCommands::kDriveCount]    = { IDM_DISK_SALVAGE1,      IDM_DISK_SALVAGE2,      IDM_DISK_SALVAGE3,      IDM_DISK_SALVAGE4      };
static constexpr WORD  s_kInspect[DiskDriveCommands::kDriveCount]    = { IDM_DISK_INSPECT1,      IDM_DISK_INSPECT2,      IDM_DISK_INSPECT3,      IDM_DISK_INSPECT4      };
static constexpr WORD  s_kThumbnail[DiskDriveCommands::kDriveCount]  = { IDM_DISK_THUMBNAIL1,    IDM_DISK_THUMBNAIL2,    IDM_DISK_THUMBNAIL3,    IDM_DISK_THUMBNAIL4    };





////////////////////////////////////////////////////////////////////////////////
//
//  GetTable
//
////////////////////////////////////////////////////////////////////////////////

static const WORD * GetTable (DiskDriveCommands::Kind kind)
{
    const WORD  * table = s_kInsert;



    switch (kind)
    {
        case DiskDriveCommands::Kind::Insert:            table = s_kInsert;    break;
        case DiskDriveCommands::Kind::Eject:             table = s_kEject;     break;
        case DiskDriveCommands::Kind::ImageWriteProtect: table = s_kWp;        break;
        case DiskDriveCommands::Kind::UserWriteProtect:  table = s_kUserWp;    break;
        case DiskDriveCommands::Kind::Salvage:           table = s_kSalvage;   break;
        case DiskDriveCommands::Kind::Inspect:           table = s_kInspect;   break;
        case DiskDriveCommands::Kind::Thumbnail:         table = s_kThumbnail; break;
    }

    return table;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskDriveCommands::GetCommand
//
////////////////////////////////////////////////////////////////////////////////

WORD DiskDriveCommands::GetCommand (Kind kind, int drive)
{
    return (drive >= 0 && drive < kDriveCount) ? GetTable (kind)[drive] : 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskDriveCommands::TryGetDrive
//
////////////////////////////////////////////////////////////////////////////////

bool DiskDriveCommands::TryGetDrive (WORD command, Kind & outKind, int & outDrive)
{
    static constexpr Kind  s_kKinds[] = { Kind::Insert, Kind::Eject, Kind::ImageWriteProtect, Kind::UserWriteProtect };
    bool                   found      = false;



    for (Kind kind : s_kKinds)
    {
        for (int drive = 0; !found && drive < kDriveCount; drive++)
        {
            if (GetTable (kind)[drive] == command)
            {
                outKind  = kind;
                outDrive = drive;
                found    = true;
            }
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DiskDriveCommands::GetMenuDrive
//
//  A user write-protect command is a Settings change, not a menu row.
//
////////////////////////////////////////////////////////////////////////////////

int DiskDriveCommands::GetMenuDrive (WORD command)
{
    static constexpr Kind  s_kRows[]  = { Kind::Insert, Kind::Eject, Kind::ImageWriteProtect, Kind::Salvage, Kind::Inspect, Kind::Thumbnail };
    int                    menuDrive  = -1;



    for (Kind kind : s_kRows)
    {
        for (int drive = 0; menuDrive < 0 && drive < kDriveCount; drive++)
        {
            menuDrive = (GetTable (kind)[drive] == command) ? drive : -1;
        }
    }

    return menuDrive;
}
