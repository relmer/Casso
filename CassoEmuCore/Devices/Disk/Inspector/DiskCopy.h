#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/TrackCopy.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskCopy
//
//  A whole disk as it was at one moment: every track record, which record
//  each quarter track plays, the records that could not be read, the file's
//  own structure, and the write-protect causes. Made on the thread that owns
//  the image (the emulation thread in Casso), then read anywhere; records the
//  guest has not touched since an earlier copy are shared with it.
//
////////////////////////////////////////////////////////////////////////////////

class DiskCopy
{
public:
    static std::shared_ptr<const DiskCopy>  MakeFromImage (const DiskImage & image, uint64_t mediaId, const std::string & fileName, uint64_t fileSize, bool isReadOnly);

    uint64_t                                        mediaId        = 0;
    DiskFormat                                      format         = DiskFormat::Dsk;
    std::string                                     fileName;
    uint64_t                                        fileSize       = 0;
    bool                                            isReadOnly     = false;
    std::array<int, DiskImage::kQuarterTrackCount>  playedSlot     = {};
    std::array<int, DiskImage::kQuarterTrackCount>  mappedSlot     = {};
    vector<std::shared_ptr<const TrackCopy>>        tracks;
    vector<DamagedTrack>                            damagedTracks;
    vector<DamagedQuarterTrack>                     damagedQuarterTracks;
    bool                                            hasCrcMismatch = false;
    WozMetadata                                     woz;
    WriteProtectInfo                                writeProtect;

    bool  IsSlotDamaged (int slot) const;
};
