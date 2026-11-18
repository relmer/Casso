#include "Pch.h"

#include "Shell/DiskDriveCommands.h"
#include "resource.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DiskDriveCommandsTests
//
//  The commands posted to the CPU thread for each of the four Disk II
//  drives, and the menu drive each Disk menu row acts on.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DiskDriveCommandsTests)
{
public:

    TEST_METHOD (GetCommand_EachDrive_RoundTrips)
    {
        using Kind = DiskDriveCommands::Kind;

        for (Kind kind : { Kind::Insert, Kind::Eject, Kind::ImageWriteProtect, Kind::UserWriteProtect })
        {
            for (int drive = 0; drive < DiskDriveCommands::kDriveCount; drive++)
            {
                WORD  command  = DiskDriveCommands::GetCommand (kind, drive);
                Kind  outKind  = Kind::Insert;
                int   outDrive = -1;



                Assert::AreNotEqual<WORD> (0, command);
                Assert::IsTrue (DiskDriveCommands::TryGetDrive (command, outKind, outDrive));
                Assert::IsTrue (outKind == kind);
                Assert::AreEqual (drive, outDrive);
            }
        }
    }


    TEST_METHOD (GetCommand_DrivesOneAndTwo_AreTheApple2Commands)
    {
        using Kind = DiskDriveCommands::Kind;

        Assert::AreEqual<WORD> (IDM_DISK_INSERT1,       DiskDriveCommands::GetCommand (Kind::Insert,            0));
        Assert::AreEqual<WORD> (IDM_DISK_INSERT2,       DiskDriveCommands::GetCommand (Kind::Insert,            1));
        Assert::AreEqual<WORD> (IDM_DISK_EJECT2,        DiskDriveCommands::GetCommand (Kind::Eject,             1));
        Assert::AreEqual<WORD> (IDM_DISK_WP1,           DiskDriveCommands::GetCommand (Kind::ImageWriteProtect, 0));
        Assert::AreEqual<WORD> (IDM_DISK_WRITEPROTECT2, DiskDriveCommands::GetCommand (Kind::UserWriteProtect,  1));
    }


    TEST_METHOD (GetCommand_DriveOutOfRange_IsZero)
    {
        Assert::AreEqual<WORD> (0, DiskDriveCommands::GetCommand (DiskDriveCommands::Kind::Insert, -1));
        Assert::AreEqual<WORD> (0, DiskDriveCommands::GetCommand (DiskDriveCommands::Kind::Insert, DiskDriveCommands::kDriveCount));
    }


    TEST_METHOD (TryGetDrive_MenuOnlyRow_IsNotAPostedCommand)
    {
        DiskDriveCommands::Kind  kind  = DiskDriveCommands::Kind::Insert;
        int                      drive = -1;



        Assert::IsFalse (DiskDriveCommands::TryGetDrive (IDM_DISK_SALVAGE4, kind, drive));
        Assert::IsFalse (DiskDriveCommands::TryGetDrive (IDM_DISK_OPEN_CASSO_EXPLORER, kind, drive));
    }


    TEST_METHOD (GetMenuDrive_EveryRow_IsItsDrive)
    {
        Assert::AreEqual (0, DiskDriveCommands::GetMenuDrive (IDM_DISK_THUMBNAIL1));
        Assert::AreEqual (1, DiskDriveCommands::GetMenuDrive (IDM_DISK_SALVAGE2));
        Assert::AreEqual (2, DiskDriveCommands::GetMenuDrive (IDM_DISK_INSPECT3));
        Assert::AreEqual (3, DiskDriveCommands::GetMenuDrive (IDM_DISK_INSERT4));
        Assert::AreEqual (3, DiskDriveCommands::GetMenuDrive (IDM_DISK_WP4));
        Assert::AreEqual (-1, DiskDriveCommands::GetMenuDrive (IDM_DISK_WRITEPROTECT1), L"a Settings change, not a menu row");
    }
};
