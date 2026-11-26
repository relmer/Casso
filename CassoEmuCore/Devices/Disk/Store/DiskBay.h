#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"
#include "Devices/Disk/MountedImageState.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskBay
//
//  One (slot, drive) position in the store and the image mounted there.
//
////////////////////////////////////////////////////////////////////////////////

struct DiskBay
{
    unique_ptr<DiskImage>  image;
    string                 path;
    DiskFormat             format  = DiskFormat::Dsk;
    bool                   mounted = false;

    //  Whether salvage applies to this image, decided once at mount.
    //  Answering it needs a full decode for a damaged disk, which is far
    //  too much work to repeat every time a menu is drawn.
    bool                   salvageOffered = false;

    //  What this bay knows about the file behind it. Set at mount, cleared
    //  at eject, refreshed after every commit this store makes.
    MountedImageState      sharedState;

    //  Which bay this is.
    //
    //  KEPT ON THE ENTRY BECAUSE FlushEntry NEEDS IT AND HAS ONLY THIS.
    //  Every message about a disk names the drive it is in, and threading
    //  a pair of ints through five call sites to reach one function is a
    //  worse answer than the entry knowing where it lives.
    int                    slot    = 0;
    int                    drive   = 0;

    //  Which disk this is, for a viewer that must tell one disk from the
    //  next: a new number at every bay change and every reload from the
    //  file, never reused in a session, and 0 while nothing has mounted.
    uint64_t               mediaId = 0;

    //  For a kept disk, the position at which it left its bay.
    uint64_t               retiredAt = 0;
};
