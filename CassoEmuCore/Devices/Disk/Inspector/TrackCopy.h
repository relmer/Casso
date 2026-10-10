#pragma once

#include "Pch.h"

#include "Devices/Disk/DiskImage.h"





////////////////////////////////////////////////////////////////////////////////
//
//  TrackCopy
//
//  One track record as it was at one moment: a bit track's packed cells and
//  bit count, or a flux track's bytes exactly as the image holds them. The
//  disk inspector and the shared sector writer read copies, never a live
//  image, so nothing they do can race the drive.
//
//  Immutable once made, and passed around as shared_ptr<const TrackCopy>, so
//  a record the guest has not touched since the last copy is shared rather
//  than copied again.
//
////////////////////////////////////////////////////////////////////////////////

struct TrackCopy
{
    int           slot            = -1;
    TrackKind     kind            = TrackKind::Bits;
    vector<Byte>  bits;
    size_t        bitCount        = 0;
    vector<Byte>  fluxBytes;
    uint64_t      guestWriteCount = 0;

    static std::shared_ptr<const TrackCopy>  MakeFromImage (const DiskImage & image, int slot);
};
