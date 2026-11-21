#pragma once

#include "Pch.h"

#include "Ui/DiskInspector/NibbleSections.h"
#include "Ui/DiskInspector/StripGeometry.h"

struct FileMap;





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest
//
//  Where a track's nibbles, cells, sectors and write seam lie across the
//  strip, and what is under a point. The turns of each nibble and cell are
//  computed once for each track, and the stretches of one kind once for
//  each track's analysis.
//
////////////////////////////////////////////////////////////////////////////////

class StripHitTest
{
public:
    static constexpr float  kValuePadDip    = 3.0f;
    static constexpr float  kCellsPerNibble = 8.0f;
    static constexpr float  kCellDigitDip   = 9.0f;
    static constexpr float  kCellTimingDip  = 30.0f;
    static constexpr float  kDigitsEstDip   = 14.0f;

    //  The turns of the track's nibbles and cells, once for each track.
    void  Update (const TrackAnalysis & track);

    //  The stretches of one kind on a quarter track, once for each track.
    void  UpdateSections (const TrackAnalysis & track, const DiskAnalysis & analysis, int quarterTrack);

    const vector<double>         & GetTurns     () const { return m_turns; }
    const vector<double>         & GetCellTurns () const { return m_cellTurns; }
    const vector<NibbleSection>  & GetSections  () const { return m_sections; }

    //  The nibble or the cell at a point across the strip, or -1 for no
    //  nibble.
    int         GetNibbleAt (const StripGeometry & g, float xPx) const;
    static int  GetCellAt   (const TrackAnalysis & track, const StripGeometry & g, float xPx);

    //  A sector's stretch from its address prologue to the end of its data
    //  epilogue, as turns and as its segments in view. Returns the segments'
    //  count.
    int  GetSectorParts (const TrackAnalysis & track, const StripGeometry & g, int sectorIndex, double & outStart, double & outEnd,
                         std::array<StripSegment, 2> & outParts) const;

    //  The sector whose bracket is under a point, or -1.
    int  GetSectorAt (const TrackAnalysis & track, const StripGeometry & g, float xPx) const;

    //  The longest sync run's segments in view, and whether a point is over
    //  them.
    int   GetSeamSegments (const TrackAnalysis & track, const StripGeometry & g, std::array<StripSegment, 2> & outParts) const;
    bool  IsOverSeam      (const TrackAnalysis & track, const StripGeometry & g, float xPx) const;

    //  Where a file's sectors lie across the strip on a whole track, a run
    //  of them as one part, in order from left to right.
    void  GetFileParts (const TrackAnalysis & track, const StripGeometry & g, const FileMap * map, int quarterTrack, int file, vector<StripSegment> & outParts) const;

    //  The first and last cells in view.
    void  GetCellsInView (const TrackAnalysis & track, double start, double span, uint32_t & outFirst, uint32_t & outLast) const;

    //  True for a flux track with every cell's timing.
    static bool  IsTimed (const TrackAnalysis & track);

    //  How wide a cell of the mean length is across `widthPx` at `span`.
    static float  GetCellPx (const TrackAnalysis & track, double span, float widthPx);

    //  The strip's zooms, as the whole track over the strip's width: by
    //  fours while the nibbles are too narrow for their values, then where
    //  the values fit, where each cell's bit fits and a step past it, and
    //  for a flux track where each cell's timing fits. `digitsPx` is the
    //  width of a value's two digits, or 0 before it is measured.
    static vector<double>  GetZoomStops (const TrackAnalysis & track, float widthPx, float digitsPx, const DxuiDpiScaler & scaler);

private:
    const TrackAnalysis    * m_turnsOf    = nullptr;
    const TrackAnalysis    * m_sectionsOf = nullptr;
    vector<double>           m_turns;
    vector<double>           m_cellTurns;
    vector<NibbleSection>    m_sections;
};
