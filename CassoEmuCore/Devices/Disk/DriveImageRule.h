#pragma once

#include "Pch.h"

#include "Devices/Disk/ImageClassifier.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ReceivingDrive
//
//  A drive an image goes to, by the images it mounts: a Disk II or a //c
//  drive, an Apple 3.5 Drive or a //c Plus drive, a UniDisk 3.5, an HD20SC
//  or a ProFile, or the cassette recorder. maxBlocks is a hard disk's
//  capacity and 0 for every other kind.
//
////////////////////////////////////////////////////////////////////////////////

enum class ReceivingDriveKind
{
    Floppy525,
    Apple35,
    UniDisk35,
    HardDisk,
    Recorder,
};


struct ReceivingDrive
{
    ReceivingDriveKind  kind      = ReceivingDriveKind::Floppy525;
    uint32_t            maxBlocks = 0;

    bool  operator== (const ReceivingDrive &) const = default;
};





////////////////////////////////////////////////////////////////////////////////
//
//  DriveImageRule
//
//  Whether a drive mounts an image, from the image's class. The picker, the
//  drop cursor and a drop all call Accepts, and a test holds it to what each
//  drive's mount does.
//
////////////////////////////////////////////////////////////////////////////////

class DriveImageRule
{
public:
    static bool                       Accepts          (const ReceivingDrive & drive, const ImageClass & imageClass);

    //  What the picker lists for the drive, as "400K and 800K disk images".
    static std::wstring               GetAcceptedText  (const ReceivingDrive & drive);

    //  The same as separate terms, as 3.5", 400K and 800K.
    static std::vector<std::wstring>  GetAcceptedTerms (const ReceivingDrive & drive);
};
