#include "Pch.h"

#include "CassoExplorer/Model/LinkImageDropRule.h"
#include "Devices/Disk/DriveImageRule.h"





////////////////////////////////////////////////////////////////////////////////
//
//  LinkImageDropRule::GetDrop
//
//  A 5.25-inch drive takes any 5.25-inch image, which is always the disk's
//  size. A block device is written from block images only; any other image
//  goes onto its volume as a file.
//
////////////////////////////////////////////////////////////////////////////////

LinkImageDrop LinkImageDropRule::GetDrop (const ImageClass & image, const LinkTreeEntry::Drive & drive)
{
    ImageLayout    layout   = image.layout;
    bool           isBlocks = layout == ImageLayout::Raw || layout == ImageLayout::TwoImg;
    LinkImageDrop  drop     = LinkImageDrop::None;



    if (!image.IsClassified())
    {
        drop = LinkImageDrop::Files;
    }
    else if (!drive.isBlockDevice)
    {
        drop = DriveImageRule::Accepts (ReceivingDrive { ReceivingDriveKind::Floppy525, 0 }, image) ? LinkImageDrop::Write : LinkImageDrop::None;
    }
    else if (!isBlocks || drive.blocks == 0)
    {
        drop = drive.empty ? LinkImageDrop::None : LinkImageDrop::Files;
    }
    else if (image.blockCount == drive.blocks)
    {
        drop = LinkImageDrop::Write;
    }
    else if (image.blockCount < drive.blocks)
    {
        drop = drive.empty ? LinkImageDrop::Write : LinkImageDrop::Choose;
    }

    return drop;
}
