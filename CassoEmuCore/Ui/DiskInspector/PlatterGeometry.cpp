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

PlatterHit PlatterGeometry::HitTest (const PlatterPlacement & view, POINT pointPx)
{
    PlatterHit  hit;
    double      dx = 0;
    double      dy = 0;



    if (view.outerRadiusPx > 0 && Unproject (view, pointPx.x + 0.5, pointPx.y + 0.5, dx, dy))
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
//  The field whose cells contain the given one, from its prologue to the end
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
//  The sector one of whose fields contains the cell: that sector, not the
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
//  The nibble whose cells contain the given one: the last that starts at or
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





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetVisibleRings
//
//  From the ring at the rectangle's farthest corner to the one at its
//  nearest point, clamped to the disk.
//
////////////////////////////////////////////////////////////////////////////////

void PlatterGeometry::GetVisibleRings (const PlatterPlacement & view, const RECT & boundsPx, int & outFirst, int & outLast)
{
    std::array<std::pair<double, double>, 4>  corners = {};
    bool                                      isPlane = GetPlaneCorners (view, boundsPx, corners);
    bool                                      isIn    = view.centerXPx >= boundsPx.left && view.centerXPx <= boundsPx.right &&
                                                        view.centerYPx >= boundsPx.top  && view.centerYPx <= boundsPx.bottom;
    double                                    r       = std::max (static_cast<double> (view.outerRadiusPx), 1.0);
    double                                    inner   = isIn ? 0.0 : std::numeric_limits<double>::max();
    double                                    outer   = 0.0;
    double                                    ex      = 0;
    double                                    ey      = 0;
    double                                    t       = 0;
    size_t                                    k       = 0;



    //  The farthest corner, and the nearest point of the corners' outline
    //  on the disk's plane when the center is outside it.
    for (k = 0; isPlane && k < corners.size(); k++)
    {
        const auto &  a = corners[k];
        const auto &  b = corners[(k + 1) % corners.size()];

        ex     = b.first - a.first;
        ey     = b.second - a.second;
        t      = std::clamp (-(a.first * ex + a.second * ey) / std::max (ex * ex + ey * ey, 1e-9), 0.0, 1.0);
        outer  = std::max (outer, std::hypot (a.first, a.second) / r);
        inner  = isIn ? inner : std::min (inner, std::hypot (a.first + t * ex, a.second + t * ey) / r);
    }

    inner = isPlane ? inner : 0.0;
    outer = isPlane ? outer : 1.0;

    outFirst = std::clamp (static_cast<int> (std::floor ((1.0 - outer) / GetRingWidth())), 0, DiskImage::kQuarterTrackCount - 1);
    outLast  = std::clamp (static_cast<int> (std::floor ((1.0 - inner) / GetRingWidth())), 0, DiskImage::kQuarterTrackCount - 1);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetVisibleTurns
//
//  A rectangle that contains the center shows the whole turn; one that does not
//  spans less than half a turn, between the angles of two of its corners.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterGeometry::GetVisibleTurns (const PlatterPlacement & view, const RECT & boundsPx, double & outStart, double & outEnd)
{
    std::array<std::pair<double, double>, 4>  corners   = {};
    bool                                      isPlane   = GetPlaneCorners (view, boundsPx, corners);
    bool                                      isPartial = isPlane && (view.centerXPx < boundsPx.left || view.centerXPx > boundsPx.right ||
                                                                      view.centerYPx < boundsPx.top  || view.centerYPx > boundsPx.bottom);
    double                                    first     = GetTurnAt (corners[0].first, corners[0].second);
    double                                    low       = 0;
    double                                    high      = 0;
    double                                    delta     = 0;



    outStart = 0.0;
    outEnd   = 1.0;

    for (const auto & corner : corners)
    {
        delta  = GetTurnAt (corner.first, corner.second) - first;
        delta -= std::round (delta);
        low    = std::min (low, delta);
        high   = std::max (high, delta);
    }

    if (isPartial)
    {
        outStart  = first + low - view.rotation;
        outStart -= std::floor (outStart);
        outEnd    = outStart + (high - low);
    }

    return isPartial;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::Project
//
//  Tilted back by t about the horizontal diameter, a point (dx, dy) is at
//  depth (dy / R) sin t toward the eye and height dy cos t, and the eye at
//  kEyeDistance radii scales it by D / (D - depth).
//
////////////////////////////////////////////////////////////////////////////////

void PlatterGeometry::Project (const PlatterPlacement & view, double dx, double dy, float & outX, float & outY)
{
    double  s = GetScaleAt (view, dy);



    outX = static_cast<float> (view.centerXPx + dx * s);
    outY = static_cast<float> (view.centerYPx + dy * std::cos (view.tilt) * s);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::ProjectAt
//
////////////////////////////////////////////////////////////////////////////////

void PlatterGeometry::ProjectAt (const PlatterPlacement & view, double turn, double radiusPx, float & outX, float & outY)
{
    static constexpr double  kTwoPi = 6.283185307179586;



    double  angle = (turn + view.rotation) * kTwoPi;



    Project (view, radiusPx * std::sin (angle), -radiusPx * std::cos (angle), outX, outY);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::Unproject
//
//  Project solved for the point: from the pixel's height y' and the depth,
//  dy = y' D R / (D R cos t + y' sin t), and dx is x' divided by the scale at dy.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterGeometry::Unproject (const PlatterPlacement & view, double x, double y, double & outDx, double & outDy)
{
    double  r     = std::max (static_cast<double> (view.outerRadiusPx), 1.0);
    double  py    = y - view.centerYPx;
    double  denom = kEyeDistance * r * std::cos (view.tilt) + py * std::sin (view.tilt);
    bool    isOn  = denom > 1e-9;



    outDy = isOn ? py * kEyeDistance * r / denom : 0.0;
    outDx = isOn ? (x - view.centerXPx) / GetScaleAt (view, outDy) : 0.0;

    return isOn;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetScaleAt
//
////////////////////////////////////////////////////////////////////////////////

double PlatterGeometry::GetScaleAt (const PlatterPlacement & view, double dy)
{
    double  r = std::max (static_cast<double> (view.outerRadiusPx), 1.0);



    return kEyeDistance / std::max (kEyeDistance - (dy / r) * std::sin (view.tilt), 1e-6);
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetLocalScale
//
//  How much larger than flat a pixel of the disk at a turn and radius is
//  drawn, the less of the scale along the ring and across it.
//
////////////////////////////////////////////////////////////////////////////////

double PlatterGeometry::GetLocalScale (const PlatterPlacement & view, double turn, double radiusPx)
{
    static constexpr double  kTwoPi = 6.283185307179586;



    float   x0    = 0.0f;
    float   y0    = 0.0f;
    float   x1    = 0.0f;
    float   y1    = 0.0f;
    float   x2    = 0.0f;
    float   y2    = 0.0f;
    double  scale = 1.0;



    if (view.tilt != 0.0f && radiusPx > 0.0)
    {
        ProjectAt (view, turn,                                     radiusPx,       x0, y0);
        ProjectAt (view, turn + 1.0 / (kTwoPi * radiusPx),         radiusPx,       x1, y1);
        ProjectAt (view, turn,                                     radiusPx + 1.0, x2, y2);

        scale = std::min (std::hypot (x1 - x0, y1 - y0), std::hypot (x2 - x0, y2 - y0));
    }

    return scale;
}





////////////////////////////////////////////////////////////////////////////////
//
//  PlatterGeometry::GetPlaneCorners
//
//  The rectangle's corners on the disk's plane; false when one is beyond
//  the horizon, where nothing can be culled.
//
////////////////////////////////////////////////////////////////////////////////

bool PlatterGeometry::GetPlaneCorners (const PlatterPlacement & view, const RECT & boundsPx, std::array<std::pair<double, double>, 4> & outCorners)
{
    const std::array<POINT, 4>  corners = { POINT { boundsPx.left, boundsPx.top }, POINT { boundsPx.right, boundsPx.top },
                                            POINT { boundsPx.right, boundsPx.bottom }, POINT { boundsPx.left, boundsPx.bottom } };
    bool                        isAll   = true;
    size_t                      k       = 0;



    for (k = 0; k < corners.size(); k++)
    {
        isAll = Unproject (view, corners[k].x, corners[k].y, outCorners[k].first, outCorners[k].second) && isAll;
    }

    return isAll;
}
