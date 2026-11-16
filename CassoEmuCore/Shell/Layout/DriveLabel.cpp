#include "Pch.h"

#include "Shell/Layout/DriveLabel.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveLabel::GetTitle
//
//  The //c's SmartPort is slot 5 to ProDOS, so a drive on its disk port
//  (slot 0 here) is addressed as slot 5. ProDOS addresses two drives a slot;
//  past those, a drive is given its unit number.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveLabel::GetTitle (const DeskDrive & drive, bool numbered)
{
    constexpr int  kCSmartPortSlot = 5;
    std::wstring   title;
    int            slot            = (drive.slot == 0) ? kCSmartPortSlot : drive.slot;



    switch (drive.model)
    {
        case DeskDriveModel::DiskII:        title = std::format (L"Drive {} - Disk ][", drive.drive + 1);                            break;
        case DeskDriveModel::Disk2c:        title = GetDisk2cTitle (drive);                                                          break;
        case DeskDriveModel::UniDisk35:     title = L"UniDisk 3.5";                                                                  break;
        case DeskDriveModel::Apple35Drive:  title = L"Apple 3.5 Drive";                                                              break;
        case DeskDriveModel::Apple35Plus:   title = L"Built-in 3.5";                                                                 break;
        case DeskDriveModel::HardDisk20SC:  title = std::format (L"HD20SC - ID {}", drive.drive);                                    break;
        case DeskDriveModel::ProFile:       title = L"ProFile";                                                                      break;
    }

    if (numbered && drive.model != DeskDriveModel::DiskII && drive.model != DeskDriveModel::Disk2c && drive.model != DeskDriveModel::HardDisk20SC)
    {
        title += (drive.drive < 2) ? std::format (L" - S{},D{}", slot, drive.drive + 1) : std::format (L" - unit {}", drive.drive + 1);
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveLabel::GetDisk2cTitle
//
//  The //c Plus's 5.25-inch drives are drives 1 and 2 of slot 6, behind its
//  built-in 3.5-inch drive in slot 5, so their slot is in the label.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveLabel::GetDisk2cTitle (const DeskDrive & drive)
{
    std::wstring  title = std::format (L"Drive {} - Disk IIc", drive.drive + 1);



    if (drive.builtIn)
    {
        title = L"Drive 1 - Built-in";
    }
    else if (drive.onPort)
    {
        title = std::format (L"Disk IIc - S6,D{}", drive.drive + 1);
    }

    return title;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveLabel::GetDetail
//
//  A floppy drive's capacity, and the disk's size when a smaller disk is in
//  it. A hard disk's size is its image's, since the emulated disk is the
//  image's size, and a ProFile's is its fixed capacity.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveLabel::GetDetail (DeskDriveModel model, uint32_t diskBlocks, uint32_t capacityBlocks)
{
    constexpr uint32_t  k35Blocks = 1600;
    std::wstring        detail;



    switch (model)
    {
        case DeskDriveModel::DiskII:
        case DeskDriveModel::Disk2c:
            detail = L"5.25\" (140K)";
            break;

        case DeskDriveModel::UniDisk35:
        case DeskDriveModel::Apple35Drive:
        case DeskDriveModel::Apple35Plus:
            detail = L"3.5\" (800K)";

            if (diskBlocks != 0 && diskBlocks < k35Blocks)
            {
                detail += std::format (L", {} disk", FormatSize (diskBlocks));
            }

            break;

        case DeskDriveModel::HardDisk20SC:
            detail = (diskBlocks != 0) ? std::format (L"Hard disk ({})", FormatSize (diskBlocks)) : std::wstring (L"Hard disk");
            break;

        case DeskDriveModel::ProFile:
            detail = std::format (L"Hard disk ({})", FormatSize (capacityBlocks));
            break;
    }

    return detail;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveLabel::FormatSize
//
//  Kilobytes below a megabyte, whole megabytes above.
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveLabel::FormatSize (uint32_t blocks)
{
    constexpr uint64_t  kBlockBytes = 512;
    constexpr uint64_t  kMegabyte   = 1024 * 1024;
    uint64_t            bytes       = (uint64_t) blocks * kBlockBytes;



    return (bytes < kMegabyte) ? std::format (L"{}K", bytes / 1024) : std::format (L"{} MB", (bytes + kMegabyte / 2) / kMegabyte);
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveLabel::GetTip
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveLabel::GetTip (const std::wstring & title, const std::wstring & detail, const std::wstring & imageName, bool isHardDisk, bool isWriteProtected)
{
    std::wstring  tip = title + L"\n" + detail + L"\n";



    if (imageName.empty())
    {
        tip += isHardDisk ? L"No image" : L"No disk";
    }
    else
    {
        tip += imageName;

        if (isWriteProtected)
        {
            tip += L"\nWrite-protected";
        }
    }

    return tip;
}
