#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"
#include "Machines/Apple2/Common/WozLoader.h"





////////////////////////////////////////////////////////////////////////////////
//
//  ComparisonTestImages
//
//  Made-up disks for the comparison's tests (SC-021), built a track at a
//  time so one track can be changed while the rest stay the same: a
//  standard track, rotated, with extra sync, retimed on a flux track, with
//  another volume, or in the 13-sector format; each disk a WOZ 2.1 in the
//  standard layout, analyzed whole.
//
////////////////////////////////////////////////////////////////////////////////

class ComparisonTestImages
{
public:
    static constexpr int   kTracks = 35;
    static constexpr Byte  kVolume = 254;

    struct TrackSpec
    {
        bool  isPresent     = true;
        bool  isFlux        = false;
        bool  isThirteen    = false;
        Byte  volume        = kVolume;
        int   rotation      = 0;
        int   extraSync     = 0;
        int   retimeFrom    = -1;
        int   changedSector = -1;
    };

    static WozSyntheticTrack  MakeTrack (int track, const TrackSpec & spec);
    static void               Analyze   (const vector<WozSyntheticTrack> & tracks, DiskAnalysis & out);

    //  A whole disk of standard tracks, with the given track changed.
    static void  AnalyzeDisk (int changedTrack, const TrackSpec & changed, DiskAnalysis & out, bool isFlux = false);
};
