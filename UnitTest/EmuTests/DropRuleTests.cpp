#include "Pch.h"

#include "Devices/Disk/DriveImageRule.h"
#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Shell/DriveDropRule.h"
#include "Ui/Scene/DeskDrivePlan.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;





////////////////////////////////////////////////////////////////////////////////
//
//  DropRuleTests
//
//  Which drive a drop tag is, and which drives take a disk while the machine
//  runs. The tag ranges are ShellDeskScene's and ShellTapeDeck's.
//
////////////////////////////////////////////////////////////////////////////////

TEST_CLASS (DropRuleTests)
{
public:

    TEST_METHOD (TagsZeroAndOneAreTheDiskIiDrives)
    {
        Assert::IsTrue  (DriveDropRule::IsDiskIiTag (0));
        Assert::IsTrue  (DriveDropRule::IsDiskIiTag (1));
        Assert::IsFalse (DriveDropRule::IsDiskIiTag (ShellTapeDeck::kTapeDropTag));
        Assert::IsFalse (DriveDropRule::IsDiskIiTag (-1));
        Assert::IsTrue  (DriveDropRule::IsRecorderTag (ShellTapeDeck::kTapeDropTag));
        Assert::IsFalse (DriveDropRule::IsRecorderTag (0));
    }



    TEST_METHOD (DeskExtrasFollowTheRecorder)
    {
        int  index = -1;



        Assert::IsTrue  (DriveDropRule::TryGetExtraIndex (ShellDeskScene::kExtraDropTagBase, index));
        Assert::AreEqual (0, index);
        Assert::IsTrue  (DriveDropRule::TryGetExtraIndex (ShellDeskScene::kExtraDropTagBase + 4, index));
        Assert::AreEqual (4, index);
        Assert::IsFalse (DriveDropRule::TryGetExtraIndex (ShellTapeDeck::kTapeDropTag, index));
        Assert::IsFalse (DriveDropRule::TryGetExtraIndex (ShellDeskScene::kBarDropTagBase, index));
    }



    TEST_METHOD (DriveBarTagsAreClassAndEntry)
    {
        int  barClass = -1;
        int  entry    = -1;



        Assert::IsTrue   (DriveDropRule::TryGetBarEntry (ShellDeskScene::kBarDropTagBase + ShellDeskScene::kBarDropClassStride + 3, barClass, entry));
        Assert::AreEqual (1, barClass);
        Assert::AreEqual (3, entry);
        Assert::IsFalse  (DriveDropRule::TryGetBarEntry (ShellDeskScene::kExtraDropTagBase, barClass, entry));
    }



    TEST_METHOD (HardDisksTakeNoDrop)
    {
        ReceivingDrive  drive;



        Assert::IsTrue  (DriveDropRule::TryGetReceivingDrive (DeskDriveModel::DiskII, drive));
        Assert::AreEqual ((int) ReceivingDriveKind::Floppy525, (int) drive.kind);
        Assert::IsTrue  (DriveDropRule::TryGetReceivingDrive (DeskDriveModel::Apple35Drive, drive));
        Assert::AreEqual ((int) ReceivingDriveKind::Apple35, (int) drive.kind);
        Assert::IsTrue  (DriveDropRule::TryGetReceivingDrive (DeskDriveModel::UniDisk35, drive));
        Assert::AreEqual ((int) ReceivingDriveKind::UniDisk35, (int) drive.kind);
        Assert::IsFalse (DriveDropRule::TryGetReceivingDrive (DeskDriveModel::HardDisk20SC, drive));
        Assert::IsFalse (DriveDropRule::TryGetReceivingDrive (DeskDriveModel::ProFile, drive));
    }
};
