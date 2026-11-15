#include "Pch.h"

#include "Devices/Disk/DriveImageRule.h"
#include "Devices/Disk/Inspector/Blocks/BlockImageFile.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DriveImageRule::Accepts
//
//  A Disk II loads sector, nibble and WOZ images, and nothing in a 2IMG or
//  Disk Copy container. The 3.5-inch drives are one mechanism, which takes
//  a 3.5-inch WOZ and an 800- or 1,600-block image in any block container.
//  A hard disk takes blocks up to its capacity, except a floppy's size,
//  which the classifier has already made a floppy.
//
////////////////////////////////////////////////////////////////////////////////

bool DriveImageRule::Accepts (const ReceivingDrive & drive, const ImageClass & imageClass)
{
    ImageContainer  container = imageClass.container;
    bool            isSector  = container == ImageContainer::Sector || container == ImageContainer::Nibble || container == ImageContainer::Woz;
    bool            isBlocks  = container == ImageContainer::Raw || container == ImageContainer::TwoImg || container == ImageContainer::DiskCopy42;



    if (!imageClass.IsClassified())
    {
        return false;
    }

    switch (drive.kind)
    {
        case ReceivingDriveKind::Floppy525:
        {
            return imageClass.formFactor == ImageFormFactor::Floppy525 && isSector;
        }

        case ReceivingDriveKind::Apple35:
        case ReceivingDriveKind::UniDisk35:
        {
            return imageClass.formFactor == ImageFormFactor::Floppy35;
        }

        case ReceivingDriveKind::HardDisk:
        {
            return imageClass.formFactor == ImageFormFactor::HardDisk && isBlocks && imageClass.blockCount <= drive.maxBlocks;
        }

        case ReceivingDriveKind::Recorder:
        {
            return imageClass.formFactor == ImageFormFactor::Tape;
        }
    }

    return false;
}





////////////////////////////////////////////////////////////////////////////////
//
//  DriveImageRule::GetAcceptedText
//
////////////////////////////////////////////////////////////////////////////////

std::wstring DriveImageRule::GetAcceptedText (const ReceivingDrive & drive)
{
    ImageClass  capacity;



    switch (drive.kind)
    {
        case ReceivingDriveKind::Floppy525:
        {
            return L"5.25-inch disk images";
        }

        case ReceivingDriveKind::Apple35:
        case ReceivingDriveKind::UniDisk35:
        {
            return L"400K and 800K disk images";
        }

        case ReceivingDriveKind::HardDisk:
        {
            capacity.status         = ImageStatus::Classified;
            capacity.formFactor     = ImageFormFactor::HardDisk;
            capacity.formattedBytes = (uint64_t) drive.maxBlocks * BlockImageFile::kBlockBytes;

            return std::format (L"Hard disk images up to {}", ImageClassifier::FormatSize (capacity));
        }

        case ReceivingDriveKind::Recorder:
        {
            return L"Tape recordings";
        }
    }

    return std::wstring();
}
