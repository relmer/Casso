#pragma once

#include "Pch.h"

#include "Devices/Disk/Inspector/DiskAnalysis.h"





enum class NibbleSectionKind
{
    Sync,
    Address,
    Data,
    Other,
    Count,
};


//  One stretch of a track's nibbles in the Nibbles view: a field, a sync run
//  or the nibbles between them, with its header's title, its facts and the
//  rest of its details for the tooltip (FR-041, FR-042).
struct NibbleSection
{
    NibbleSectionKind  kind        = NibbleSectionKind::Other;
    int                firstNibble = 0;
    int                count       = 0;
    int                field       = -1;
    int                sectorIndex = -1;
    int                sector      = -1;
    std::wstring       title;
    std::wstring       facts;
    std::wstring       details;
};





////////////////////////////////////////////////////////////////////////////////
//
//  NibbleSections
//
//  Splits a track's nibbles into sections in track order: each address and
//  data field, each sync run, and each other stretch between them. It is
//  separate from the view so it can be tested.
//
////////////////////////////////////////////////////////////////////////////////

class NibbleSections
{
public:
    static vector<NibbleSection>  Build (const DiskAnalysis & analysis, int quarterTrack);

    //  A section's header as one line of text, for Copy (FR-057).
    static std::wstring  FormatHeader (const NibbleSection & section);

private:
    static void      AddGap      (const TrackAnalysis & track, int first, int end, vector<NibbleSection> & inOut);
    static void      AddStretch  (const TrackAnalysis & track, NibbleSectionKind kind, int first, int end, vector<NibbleSection> & inOut);
    static void      AddField    (const DiskAnalysis & analysis, int quarterTrack, const TrackAnalysis & track, int field, vector<NibbleSection> & inOut);
    static uint32_t  GetCellAt   (const TrackAnalysis & track, int nibble);
};
