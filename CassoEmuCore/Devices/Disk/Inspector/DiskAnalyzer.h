#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





////////////////////////////////////////////////////////////////////////////////
//
//  DiskAnalyzer
//
//  Analyzes every record a quarter track plays (or would play, for a damaged
//  one), once each, then builds what needs the whole disk: the quarter-track
//  entries, the findings that compare tracks (volume, 13-sector fields beside
//  16-sector ones, record layout, track length, damage, the image file), and
//  the summary. Reanalyze replaces only the records given, as the Casso host
//  does after the guest writes.
//
////////////////////////////////////////////////////////////////////////////////

class DiskAnalyzer
{
public:
    //  A WOZ bit or flux track more than this far from nominal is reported.
    static constexpr double  kTrackLengthTolerance = 0.02;

    static void          Analyze    (std::shared_ptr<const DiskCopy> copy, const DecodeSettings & settings, DiskAnalysis & out);
    static void          Reanalyze  (std::shared_ptr<const DiskCopy> copy, std::span<const int> slots, DiskAnalysis & inOut);
    static TrackContext  GetContext (const DiskCopy & copy, int slot);
    static vector<int>   GetQuarterTracksOfSlot (const DiskCopy & copy, int slot);

private:
    static void  Assemble             (DiskAnalysis & inOut);
    static void  BuildEntries         (DiskAnalysis & inOut);
    static void  AddTrackFindings     (DiskAnalysis & inOut);
    static void  AddVolumeFindings    (DiskAnalysis & inOut);
    static void  AddLayoutFindings    (DiskAnalysis & inOut);
    static void  AddLengthFindings    (DiskAnalysis & inOut);
    static void  AddDamageFindings    (DiskAnalysis & inOut);
    static void  AddImageFindings     (DiskAnalysis & inOut);
    static void  Summarize            (DiskAnalysis & inOut);
    static int   GetHomeQuarterTrack  (const DiskCopy & copy, int slot);
};
