#include "Pch.h"

#include "Ui/DiskInspector/PlatterGeometry.h"
#include "Devices/Disk/Inspector/TrackAnalyzer.h"





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetRingWidth
//
////////////////////////////////////////////////////////////////////////////////

double PlatterGeometry::GetRingWidth()
{
    return (1.0 - PlatterRenderer::kInnerFraction) / PlatterRenderer::kRingCount;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetRingOuter
//
////////////////////////////////////////////////////////////////////////////////

double PlatterGeometry::GetRingOuter (int quarterTrack)
{
    return 1.0 - quarterTrack * GetRingWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetRingMiddle
//
////////////////////////////////////////////////////////////////////////////////

double PlatterGeometry::GetRingMiddle (int quarterTrack)
{
    return 1.0 - (quarterTrack + 0.5) * GetRingWidth();
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetQuarterTrackAt
//
//  -1 off the rim or in the hub.
//
////////////////////////////////////////////////////////////////////////////////

int PlatterGeometry::GetQuarterTrackAt (double radius)
{
    int  quarterTrack = -1;



    if (radius <= 1.0 && radius >= PlatterRenderer::kInnerFraction)
    {
        quarterTrack = std::min (static_cast<int> ((1.0 - radius) / GetRingWidth()), PlatterRenderer::kRingCount - 1);
    }

    return quarterTrack;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetTurnAt
//
//  dx grows rightward and dy downward, as on screen.
//
////////////////////////////////////////////////////////////////////////////////

double PlatterGeometry::GetTurnAt (double dx, double dy)
{
    static constexpr double  kTwoPi = 6.283185307179586;



    double  turn = std::atan2 (dx, -dy) / kTwoPi;

    return turn < 0 ? turn + 1.0 : turn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::IsGrooveOutside
//
//  A groove runs along the outer edge of each whole track's ring.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterGeometry::IsGrooveOutside (int quarterTrack)
{
    return quarterTrack % DiskImage::kQuarterTracksPerWholeTrack == 0;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::HitTest
//
////////////////////////////////////////////////////////////////////////////////

PlatterHit PlatterGeometry::HitTest (const PlatterView & view, POINT pointPx)
{
    PlatterHit  hit;
    double      dx = pointPx.x + 0.5 - view.centerXPx;
    double      dy = pointPx.y + 0.5 - view.centerYPx;



    if (view.outerRadiusPx > 0)
    {
        hit.quarterTrack = GetQuarterTrackAt (std::hypot (dx, dy) / view.outerRadiusPx);
        hit.isOnDisk     = hit.quarterTrack >= 0;
        hit.turn         = GetTurnAt (dx, dy) - view.rotation;
        hit.turn        -= std::floor (hit.turn);
    }

    return hit;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetCellAtTurn
//
//  The inverse of TrackAnalyzer::GetAngle: linear on a bit track, by the
//  cells' own times on a flux track.
//
////////////////////////////////////////////////////////////////////////////////

uint32_t PlatterGeometry::GetCellAtTurn (const TrackAnalysis & track, double turn)
{
    const FramedTrack &  framed  = track.framed;
    uint32_t             cell    = 0;
    double               target  = 0;
    double               elapsed = 0;



    turn -= std::floor (turn);

    if (framed.cellCount == 0)
    {
        cell = 0;
    }
    else if (framed.isFlux && framed.turnTicks > 0 && framed.cellTicks.size() >= framed.cellCount)
    {
        target = turn * framed.turnTicks;

        while (cell + 1 < framed.cellCount && elapsed + framed.cellTicks[cell] <= target)
        {
            elapsed += framed.cellTicks[cell];
            cell++;
        }
    }
    else
    {
        cell = std::min (static_cast<uint32_t> (turn * framed.cellCount), framed.cellCount - 1);
    }

    return cell;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetFieldAt
//
//  The field whose cells hold the given one, from its prologue to the end
//  of its epilogue, or -1 between fields.
//
////////////////////////////////////////////////////////////////////////////////

int PlatterGeometry::GetFieldAt (const TrackAnalysis & track, uint32_t cell)
{
    int     field = -1;
    size_t  i     = 0;



    for (i = 0; i < track.fields.size() && field < 0; i++)
    {
        if (IsCellInSpan (cell, track.fields[i].startCell, track.fields[i].endCell, track.framed.cellCount))
        {
            field = static_cast<int> (i);
        }
    }

    return field;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetSectorAt
//
//  The sector one of whose fields holds the cell: that sector, not the
//  first with the same number.
//
////////////////////////////////////////////////////////////////////////////////

int PlatterGeometry::GetSectorAt (const TrackAnalysis & track, uint32_t cell)
{
    int     field  = GetFieldAt (track, cell);
    int     sector = -1;
    size_t  i      = 0;



    for (i = 0; i < track.sectors.size() && field >= 0 && sector < 0; i++)
    {
        if (track.sectors[i].addressField == field || track.sectors[i].dataField == field)
        {
            sector = static_cast<int> (i);
        }
    }

    return sector;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetNibbleAt
//
//  The nibble whose cells hold the given one: the last that starts at or
//  before it, or the last nibble of the track for the cells before the first.
//
////////////////////////////////////////////////////////////////////////////////

int PlatterGeometry::GetNibbleAt (const TrackAnalysis & track, uint32_t cell)
{
    const vector<FramedNibble> &  nibbles = track.framed.nibbles;
    int                           nibble  = -1;



    if (!nibbles.empty())
    {
        auto  after = std::upper_bound (nibbles.begin(), nibbles.end(), cell,
                                        [] (uint32_t c, const FramedNibble & n) { return c < n.startCell; });

        nibble = (after == nibbles.begin()) ? static_cast<int> (nibbles.size()) - 1 : static_cast<int> (after - nibbles.begin()) - 1;
    }

    return nibble;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::IsCellInSpan
//
//  Whether a cell lies in [start, end), a span that may wrap past the index.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterGeometry::IsCellInSpan (uint32_t cell, uint32_t start, uint32_t end, uint32_t count)
{
    bool  isIn = false;



    if (count > 0)
    {
        cell  %= count;
        start %= count;
        end   %= count;
        isIn   = (start <= end) ? (cell >= start && cell < end) : (cell >= start || cell < end);
    }

    return isIn;
}
