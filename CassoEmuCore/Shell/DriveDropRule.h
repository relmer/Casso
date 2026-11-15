#pragma once

#include "Pch.h"

#include "Devices/Disk/DriveImageRule.h"

enum class DeskDriveModel;





////////////////////////////////////////////////////////////////////////////////
//
//  DriveAddress
//
//  A drive the shell inserts into: a card's slot and the drive on it, or the
//  cassette recorder.
//
////////////////////////////////////////////////////////////////////////////////

struct DriveAddress
{
    int   slot       = 0;
    int   drive      = 0;
    bool  isRecorder = false;

    bool  operator== (const DriveAddress &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DriveDropRule
//
//  What a drop tag is, and which drives take a disk while the machine runs.
//  Tags 0 and 1 are the Disk II drives in slot 6, the recorder has its own
//  tag, the desk's other drives follow it, and the drive bar's come last in
//  blocks of one class each.
//
//  A hard disk's image is set in Settings and attached when the machine is
//  built, so no hard disk takes a drop.
//
////////////////////////////////////////////////////////////////////////////////

class DriveDropRule
{
public:
    static constexpr int  kDiskIiSlot = 6;

    static bool  TryGetReceivingDrive (DeskDriveModel model, ReceivingDrive & outDrive);
    static bool  IsDiskIiTag          (int tag);
    static bool  IsRecorderTag        (int tag);
    static bool  TryGetExtraIndex     (int tag, int & outIndex);
    static bool  TryGetBarEntry       (int tag, int & outClass, int & outEntry);
};
