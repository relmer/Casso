#pragma once

#include "Pch.h"

#include "Ui/Scene/DeskDrivePlan.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveLabel
//
//  What a drive is called on the drive bar and in its tip on the desk: two
//  lines, its name, and its form factor and capacity. A Disk II is numbered
//  as DOS numbers it; an HD20SC's name has its SCSI ID. A 3.5-inch drive or
//  ProFile has its ProDOS slot and drive added when `numbered`, for a class
//  with two of the same kind.
//
////////////////////////////////////////////////////////////////////////////////

class DriveLabel
{
public:
    //  `diskBlocks` is the mounted disk's or image's size, 0 for none;
    //  `capacityBlocks` is a ProFile's fixed size.
    static std::wstring  GetTitle   (const DeskDrive & drive, bool numbered);
    static std::wstring  GetDetail  (DeskDriveModel model, uint32_t diskBlocks, uint32_t capacityBlocks);
    static std::wstring  FormatSize (uint32_t blocks);

    //  A desk drive's tip: its two lines, then its image's name, or "No disk"
    //  ("No image" for a hard disk), then whether the image is write-protected.
    static std::wstring  GetTip     (const std::wstring & title,
                                     const std::wstring & detail,
                                     const std::wstring & imageName,
                                     bool                 isHardDisk,
                                     bool                 isWriteProtected);

private:
    static std::wstring  GetDisk2cTitle (const DeskDrive & drive);
};
