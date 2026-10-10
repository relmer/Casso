#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/FileMap/FileMap.h"
#include "Devices/Disk/Inspector/ImageDetails.h"
#include "Devices/Disk/Inspector/TrackAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalysis and its parts
//
//  Everything the disk inspector shows about one disk: an entry per quarter
//  track, an analysis per track record (each record once, however many
//  quarter tracks play it), every finding in quarter-track and cell order,
//  the summary behind the chips, and the image file's own details.
//
////////////////////////////////////////////////////////////////////////////////

enum class QuarterTrackContent
{
    Nothing,
    BitTrack,
    FluxTrack,
    Damaged,
};


struct QuarterTrackEntry
{
    QuarterTrackContent  content           = QuarterTrackContent::Nothing;
    int                  slot              = -1;
    bool                 isFromFluxMap     = false;
    vector<int>          sharesWith;
    TrackClass           trackClass        = TrackClass::NothingRecorded;
    int                  sectorsGood       = 0;
    int                  sectorsFound      = 0;
    int                  sectorsNotChecked = 0;
    bool                 isBeyondHeadReach = false;
    bool                 hasDamageReason   = false;
    DamageReason         damageReason      = DamageReason::OutsideFile;
    int                  findingCount      = 0;
};


enum class DiskFormatClass
{
    NothingRecorded,
    ThirteenAndSixteen,
    Sixteen,
    Thirteen,
    Nonstandard,
    Unformatted,
    Damaged,
};


struct DiskSummary
{
    DiskFormatClass  format            = DiskFormatClass::NothingRecorded;
    int              tracksWithData    = 0;
    int              sectorsGood       = 0;
    int              sectorsFound      = 0;
    int              sectorsNotChecked = 0;
    int              badSectors        = 0;
    int              nonstandardTracks = 0;
    int              unformattedTracks = 0;
    int              fluxTracks        = 0;
    int              damagedTracks     = 0;
    int              commonVolume      = -1;
};


struct DiskAnalysis
{
    uint64_t                                                      mediaId   = 0;
    std::shared_ptr<const DiskCopy>                               copy;
    DecodeSettings                                                settings;
    std::array<QuarterTrackEntry, DiskImage::kQuarterTrackCount>  entries;
    vector<std::shared_ptr<const TrackAnalysis>>                  tracks;
    vector<Finding>                                               findings;
    DiskSummary                                                   summary;
    ImageDetails                                                  image;
    vector<FileMap>                                               fileMaps;
    int                                                           headLimit = 0;
};
