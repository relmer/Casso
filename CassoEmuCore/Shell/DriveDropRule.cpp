#include "Pch.h"

#include "Shell/DriveDropRule.h"

#include "Shell/Components/ShellDeskScene.h"
#include "Shell/Components/ShellTapeDeck.h"
#include "Ui/Scene/DeskDrivePlan.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveDropRule::TryGetReceivingDrive
//
////////////////////////////////////////////////////////////////////////////////

bool DriveDropRule::TryGetReceivingDrive (DeskDriveModel model, ReceivingDrive & outDrive)
{
    outDrive = ReceivingDrive { DeskDrivePlan::GetReceivingKind (model), 0 };

    return outDrive.kind != ReceivingDriveKind::HardDisk;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveDropRule::IsDiskIiTag
//
////////////////////////////////////////////////////////////////////////////////

bool DriveDropRule::IsDiskIiTag (int tag)
{
    return tag >= 0 && tag < ShellTapeDeck::kTapeDropTag;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveDropRule::IsRecorderTag
//
////////////////////////////////////////////////////////////////////////////////

bool DriveDropRule::IsRecorderTag (int tag)
{
    return tag == ShellTapeDeck::kTapeDropTag;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveDropRule::TryGetExtraIndex
//
////////////////////////////////////////////////////////////////////////////////

bool DriveDropRule::TryGetExtraIndex (int tag, int & outIndex)
{
    outIndex = tag - ShellDeskScene::kExtraDropTagBase;

    return tag >= ShellDeskScene::kExtraDropTagBase && tag < ShellDeskScene::kBarDropTagBase;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveDropRule::TryGetBarEntry
//
////////////////////////////////////////////////////////////////////////////////

bool DriveDropRule::TryGetBarEntry (int tag, int & outClass, int & outEntry)
{
    int  offset = tag - ShellDeskScene::kBarDropTagBase;



    outClass = offset / ShellDeskScene::kBarDropClassStride;
    outEntry = offset % ShellDeskScene::kBarDropClassStride;

    return tag >= ShellDeskScene::kBarDropTagBase;
}
