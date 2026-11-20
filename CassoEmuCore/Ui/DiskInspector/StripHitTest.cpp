#include "Pch.h"

#include "Ui/DiskInspector/StripHitTest.h"
#include "Ui/DiskInspector/FluxTiming.h"
#include "Ui/DiskInspector/InspectorMarquee.h"
#include "Ui/DiskInspector/PlatterGeometry.h"
#include "Ui/DiskInspector/ZoomStops.h"





static constexpr double  s_kCoarseRatio  = 4.0;
static constexpr double  s_kMinStepRatio = 1.2;
static constexpr double  s_kBitStepRatio = 1.25;





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::Update
//
////////////////////////////////////////////////////////////////////////////////

void StripHitTest::Update (const TrackAnalysis & track)
{
    if (m_turnsOf != &track)
    {
        StripGeometry::BuildNibbleTurns (track, m_turns);
        FluxTiming::BuildCellTurns (track, m_cellTurns);
        m_turnsOf = &track;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::UpdateSections
//
////////////////////////////////////////////////////////////////////////////////

void StripHitTest::UpdateSections (const TrackAnalysis & track, const DiskAnalysis & analysis, int quarterTrack)
{
    if (m_sectionsOf != &track)
    {
        m_sections   = NibbleSections::Build (analysis, quarterTrack);
        m_sectionsOf = &track;
    }
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::GetNibbleAt
//
////////////////////////////////////////////////////////////////////////////////

int StripHitTest::GetNibbleAt (const StripGeometry & g, float xPx) const
{
    double  turn   = g.GetTurn (xPx);
    int     nibble = -1;



    if (m_turns.size() > 1)
    {
        //  The turns rise from the first nibble's, which can be just before
        //  the index, so the turn is brought into the same lap.
        turn -= std::floor (turn);
        turn += (turn < m_turns.front()) ? 1.0 : 0.0;

        auto  after = std::upper_bound (m_turns.begin(), m_turns.end() - 1, turn);

        nibble = (after == m_turns.begin()) ? static_cast<int> (m_turns.size()) - 2 : static_cast<int> (after - m_turns.begin()) - 1;
    }

    return nibble;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::GetCellAt
//
////////////////////////////////////////////////////////////////////////////////

int StripHitTest::GetCellAt (const TrackAnalysis & track, const StripGeometry & g, float xPx)
{
    double  turn = g.GetTurn (xPx);



    turn -= std::floor (turn);

    return static_cast<int> (PlatterGeometry::GetCellAtTurn (track, turn));
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::GetSectorParts
//
////////////////////////////////////////////////////////////////////////////////

int StripHitTest::GetSectorParts (const TrackAnalysis & track, const StripGeometry & g, int sectorIndex, double & outStart, double & outEnd,
                                  std::array<StripSegment, 2> & outParts) const
{
    const AnalyzedSector &  sector = track.sectors[static_cast<size_t> (sectorIndex)];
    const LocatedField &    first  = track.fields[sector.addressField];
    const LocatedField &    last   = track.fields[sector.dataField >= 0 ? sector.dataField : sector.addressField];
    int                     end    = last.firstNibble + last.nibbleCount;



    outStart = m_turns[static_cast<size_t> (first.firstNibble)];
    outEnd   = (end < static_cast<int> (m_turns.size())) ? m_turns[static_cast<size_t> (end)] : m_turns.back();

    return g.GetSegments (outStart, outEnd - outStart - std::floor (outEnd - outStart), outParts);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::GetSectorAt
//
////////////////////////////////////////////////////////////////////////////////

int StripHitTest::GetSectorAt (const TrackAnalysis & track, const StripGeometry & g, float xPx) const
{
    std::array<StripSegment, 2>  parts  = {};
    int                          found  = -1;
    int                          count  = 0;
    double                       a      = 0;
    double                       b      = 0;



    for (int s = 0; s < static_cast<int> (track.sectors.size()) && found < 0; s++)
    {
        count = GetSectorParts (track, g, s, a, b, parts);

        for (int k = 0; k < count; k++)
        {
            found = (xPx >= parts[k].x0 && xPx < parts[k].x1) ? s : found;
        }
    }

    return found;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::GetSeamSegments
//
////////////////////////////////////////////////////////////////////////////////

int StripHitTest::GetSeamSegments (const TrackAnalysis & track, const StripGeometry & g, std::array<StripSegment, 2> & outParts) const
{
    const SyncRun &  seam  = track.measurements.longestSync;
    size_t           n     = track.framed.nibbles.size();
    double           a     = 0;
    double           b     = 0;
    int              count = 0;



    //  A run's width in cells is each sync nibble's, so its extent comes from
    //  where its first nibble starts and where the nibble after it starts.
    if (seam.count > 0 && n > 0 && m_turns.size() == n + 1)
    {
        a     = m_turns[static_cast<size_t> (seam.firstNibble) % n];
        b     = m_turns[static_cast<size_t> (seam.firstNibble + seam.count) % n];
        count = g.GetSegments (a, b - a - std::floor (b - a), outParts);
    }

    return count;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::IsOverSeam
//
////////////////////////////////////////////////////////////////////////////////

bool StripHitTest::IsOverSeam (const TrackAnalysis & track, const StripGeometry & g, float xPx) const
{
    std::array<StripSegment, 2>  parts  = {};
    int                          count  = GetSeamSegments (track, g, parts);
    int                          k      = 0;
    bool                         isOver = false;



    for (k = 0; k < count; k++)
    {
        isOver = isOver || (xPx >= parts[k].x0 && xPx < parts[k].x1);
    }

    return isOver;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::GetFileParts
//
////////////////////////////////////////////////////////////////////////////////

void StripHitTest::GetFileParts (const TrackAnalysis & track, const StripGeometry & g, const FileMap * map, int quarterTrack, int file, vector<StripSegment> & outParts) const
{
    std::array<StripSegment, 2>  parts = {};
    int                          count = 0;
    bool                         isOn  = map != nullptr && map->notMapped == NotMappedReason::None && quarterTrack % DiskImage::kQuarterTracksPerWholeTrack == 0 && file >= 0;



    for (const InspectorMarquee::Span & span : isOn ? InspectorMarquee::GetFileSpans (track, m_turns, *map, quarterTrack / DiskImage::kQuarterTracksPerWholeTrack, file)
                                                    : vector<InspectorMarquee::Span>())
    {
        count = g.GetSegments (span.a, span.b - span.a, parts);
        outParts.insert (outParts.end(), parts.begin(), parts.begin() + count);
    }

    std::sort (outParts.begin(), outParts.end(), [] (const StripSegment & x, const StripSegment & y) { return x.x0 < y.x0; });
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::GetCellsInView
//
////////////////////////////////////////////////////////////////////////////////

void StripHitTest::GetCellsInView (const TrackAnalysis & track, double start, double span, uint32_t & outFirst, uint32_t & outLast) const
{
    outFirst = (span >= 1.0) ? 0 : FluxTiming::GetCellAt (m_cellTurns, start);
    outLast  = (span >= 1.0) ? track.framed.cellCount - 1 : FluxTiming::GetCellAt (m_cellTurns, start + span);
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::IsTimed
//
////////////////////////////////////////////////////////////////////////////////

bool StripHitTest::IsTimed (const TrackAnalysis & track)
{
    return track.framed.isFlux && track.framed.turnTicks > 0 && track.framed.cellTicks.size() >= track.framed.cellCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::GetCellPx
//
////////////////////////////////////////////////////////////////////////////////

float StripHitTest::GetCellPx (const TrackAnalysis & track, double span, float widthPx)
{
    return widthPx / static_cast<float> (span * std::max<uint32_t> (track.framed.cellCount, 1));
}





////////////////////////////////////////////////////////////////////////////////
//
//  StripHitTest::GetZoomStops
//
////////////////////////////////////////////////////////////////////////////////

vector<double> StripHitTest::GetZoomStops (const TrackAnalysis & track, float widthPx, float digitsPx, const DxuiDpiScaler & scaler)
{
    double          cells   = static_cast<double> (std::max<uint32_t> (track.framed.cellCount, 1));
    double          width   = std::max (widthPx, 1.0f);
    double          digitsW = (digitsPx > 0) ? digitsPx : scaler.ToPxf (kDigitsEstDip);
    double          hexCell = (digitsW + scaler.ToPxf (kValuePadDip)) / kCellsPerNibble;
    double          bitCell = scaler.ToPxf (kCellDigitDip);
    vector<double>  details = { hexCell * cells / width, bitCell * cells / width, bitCell * s_kBitStepRatio * cells / width };



    if (IsTimed (track))
    {
        details.push_back (scaler.ToPxf (kCellTimingDip) * cells / width);
    }

    return ZoomStops::Build (1.0, s_kCoarseRatio, details, s_kMinStepRatio);
}
