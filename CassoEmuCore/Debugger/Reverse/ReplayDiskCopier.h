#pragma once

#include "Pch.h"

#include "Devices/Disk/IDiskImage.h"

class DiskImageStore;





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayDisk
//
//  A disk in one drive bay as a replay on a second machine needs it: which
//  medium it is, its track slots, and an image of its format for the second
//  machine to mount, so a snapshot's tracks load over it. A snapshot holds
//  every track but not how quarter tracks map to them, which a WOZ's own
//  file gives; a sector image maps them the standard way, so a blank one of
//  its format will do.
//
////////////////////////////////////////////////////////////////////////////////

struct ReplayDisk
{
    int                                        slot       = 0;
    int                                        drive      = 0;
    uint64_t                                   mediaId    = 0;
    int                                        trackCount = 0;
    DiskFormat                                 format     = DiskFormat::Dsk;
    std::shared_ptr<const std::vector<Byte>>   image;
};





////////////////////////////////////////////////////////////////////////////////
//
//  ReplayDiskCopier
//
//  Copies the disks in a machine's bays for a replay on a second machine,
//  on the thread that runs the first. A sector image goes as a blank one of
//  its format, shared by all, and any other as its format's file would hold
//  it, made once per medium and shared with every copy after; the images of
//  media no longer in a bay are let go.
//
////////////////////////////////////////////////////////////////////////////////

class ReplayDiskCopier
{
public:
    HRESULT  Copy (DiskImageStore & store, std::vector<ReplayDisk> & outDisks);

    //  Lets go of the images made for media, which the next copy makes again.
    void     ReleaseImages () { m_images.clear(); }

private:
    std::unordered_map<uint64_t, std::shared_ptr<const std::vector<Byte>>>  m_images;     // by medium
    std::shared_ptr<const std::vector<Byte>>                                m_blankSectorImage;
};
